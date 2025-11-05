'use client';

import { useState, useEffect, useRef } from 'react';
import { Line } from 'react-chartjs-2';
import {
  Chart as ChartJS, CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend, Filler
} from 'chart.js';

ChartJS.register( CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend, Filler );

const ESP32_IP = '192.168.1.19';
// ------------------------------------
const ESP32_DATA_URL = `http://${ESP32_IP}/data`;
const ESP32_FFT_URL = `http://${ESP32_IP}/fftdata`;
const MAX_DATA_POINTS = 30;

export default function Home() {
  const [vibrationDataX, setVibrationDataX] = useState<number[]>([]);
  const [vibrationDataY, setVibrationDataY] = useState<number[]>([]);
  const [vibrationDataZ, setVibrationDataZ] = useState<number[]>([]);
  const [labels, setLabels] = useState<string[]>([]);
  const [errorData, setErrorData] = useState<string | null>(null);
  const isFetchingData = useRef(false);

  const [peakFrequency, setPeakFrequency] = useState<number>(0);
  const [estimatedRPM, setEstimatedRPM] = useState<number>(0);
  const [errorFFT, setErrorFFT] = useState<string | null>(null);
  const isFetchingFFT = useRef(false);

  useEffect(() => {
    const fetchData = async () => {
      if (isFetchingData.current) return;
      isFetchingData.current = true;
      setErrorData(null);

      try {
        const response = await fetch(ESP32_DATA_URL);
        if (!response.ok) throw new Error(`Erro HTTP: ${response.status}`);
        const data = await response.json();

        const currentTime = new Date().toLocaleTimeString();

        setVibrationDataX((prev) => {
          const newData = [...prev, data.ax];
          if (newData.length > MAX_DATA_POINTS) newData.shift();
          return newData;
        });
        setVibrationDataY((prev) => {
          const newData = [...prev, data.ay];
          if (newData.length > MAX_DATA_POINTS) newData.shift();
          return newData;
        });
        setVibrationDataZ((prev) => {
          const newData = [...prev, data.az_comp];
          if (newData.length > MAX_DATA_POINTS) newData.shift();
          return newData;
        });
        setLabels((prev) => {
          const newLabels = [...prev, currentTime];
          if (newLabels.length > MAX_DATA_POINTS) newLabels.shift();
          return newLabels;
        });

      } catch (err) {
        console.error("Erro ao buscar dados brutos:", err);
        setErrorData(`Falha ao buscar /data (${ESP32_IP}).`);
        setVibrationDataX([]); setVibrationDataY([]); setVibrationDataZ([]); setLabels([]);
      } finally {
        isFetchingData.current = false;
      }
    };
    fetchData();
    const intervalId = setInterval(fetchData, 1000); 
    return () => clearInterval(intervalId);
  }, [ESP32_IP]);

  useEffect(() => {
    const fetchFFTData = async () => {
      if (isFetchingFFT.current) return;
      isFetchingFFT.current = true;
      setErrorFFT(null);
      try {
        const response = await fetch(ESP32_FFT_URL);
        if (!response.ok) throw new Error(`Erro HTTP: ${response.status}`);
        const data = await response.json();
        setPeakFrequency(data.peakFreq);
        setEstimatedRPM(data.peakFreq * 60); 
      } catch (err) {
        console.error("Erro ao buscar dados FFT:", err);
        setErrorFFT(`Falha ao buscar /fftdata (${ESP32_IP}).`);
        setPeakFrequency(0); setEstimatedRPM(0);
      } finally {
        isFetchingFFT.current = false;
      }
    };
    fetchFFTData();
    const intervalId = setInterval(fetchFFTData, 2000);
    return () => clearInterval(intervalId);
  }, [ESP32_IP]);


  const commonChartOptions = {
    scales: { 
      y: { beginAtZero: false, ticks: { color: '#5D4037' }, grid: { color: '#A1887F' } }, 
      x: { ticks: { display: false }, grid: { display: false } } 
    },
    plugins: { 
      legend: { display: false }
    }, 
    animation: { duration: 250 }, 
    maintainAspectRatio: false
  };

  const chartDataX = {
    labels: labels,
    datasets: [{
      label: 'Vibracao X (m/s^2)', 
      data: vibrationDataX, 
      borderColor: '#8D6E63',
      backgroundColor: 'rgba(141, 110, 99, 0.2)', 
      borderWidth: 2, fill: true, tension: 0.1 
    }],
  };

  const chartDataY = {
    labels: labels,
    datasets: [{
      label: 'Vibracao Y (m/s^2)', 
      data: vibrationDataY, 
      borderColor: '#BCAAA4',
      backgroundColor: 'rgba(188, 170, 164, 0.2)', 
      borderWidth: 2, fill: true, tension: 0.1 
    }],
  };
  
  const chartDataZ = {
    labels: labels,
    datasets: [{
      label: 'Vibracao Z (m/s^2)', 
      data: vibrationDataZ, 
      borderColor: '#5D4037',
      backgroundColor: 'rgba(93, 64, 55, 0.2)', 
      borderWidth: 2, fill: true, tension: 0.1 
    }],
  };

  return (
    <main className="flex min-h-screen flex-col items-center p-6 bg-amber-50 text-amber-900">
      <h1 className="text-4xl font-bold mb-8 text-amber-950">Monitor de Vibracao em Tempo Real</h1>

      {(errorData || errorFFT) && (
        <div className="bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded relative mb-6 w-full max-w-6xl" role="alert">
          <strong className="font-bold">Erro de Conexao:</strong><br/>
          {errorData && <span className="block sm:inline">{errorData}</span>}
          {errorFFT && <span className="block sm:inline">{errorFFT}</span>}
          <span className="block sm:inline"> Verifique o IP do ESP32 e a conexao Wi-Fi.</span>
        </div>
      )}

      {/* --- CÉLULAS / CARDS DE DADOS (FFT/RPM) --- */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-8 w-full max-w-4xl">
          <div className="bg-white p-4 rounded-lg shadow-md border border-amber-200 text-center">
              <h2 className="text-xl mb-1 text-amber-800">Frequencia de Pico</h2>
              <p className="text-4xl font-semibold text-amber-700">
                  {peakFrequency.toFixed(1)} <span className="text-lg">Hz</span>
              </p>
          </div>
          <div className="bg-white p-4 rounded-lg shadow-md border border-amber-200 text-center">
              <h2 className="text-xl mb-1 text-amber-800">RPM Estimado</h2>
              <p className="text-4xl font-semibold text-amber-700">
                  {estimatedRPM.toFixed(0)} <span className="text-lg">RPM</span>
              </p>
          </div>
      </div>
      
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 w-full max-w-6xl">
        
        <div className="bg-white p-4 rounded-lg shadow-md border border-amber-200">
          <h2 className="text-2xl font-semibold text-center mb-2" style={{color: '#8D6E63'}}>Eixo X</h2>
          <div className="h-64">
            <Line data={chartDataX} options={commonChartOptions} />
          </div>
        </div>

        <div className="bg-white p-4 rounded-lg shadow-md border border-amber-200">
          <h2 className="text-2xl font-semibold text-center mb-2" style={{color: '#BCAAA4'}}>Eixo Y</h2>
          <div className="h-64">
            <Line data={chartDataY} options={commonChartOptions} />
          </div>
        </div>

        <div className="bg-white p-4 rounded-lg shadow-md border border-amber-200">
          <h2 className="text-2xl font-semibold text-center mb-2" style={{color: '#5D4037'}}>Eixo Z</h2>
          <div className="h-64">
            <Line data={chartDataZ} options={commonChartOptions} />
          </div>
        </div>
        
      </div>
    </main>
  );
}