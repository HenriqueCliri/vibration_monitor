'use client';

import { useState, useEffect, useRef } from 'react';
import { Line, Bar } from 'react-chartjs-2'; 
import {
  Chart as ChartJS, CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend, Filler, BarElement 
} from 'chart.js';

ChartJS.register( CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend, Filler, BarElement );

const ESP32_IP = '192.168.1.5';
const ESP32_WS_URL = `ws://${ESP32_IP}/ws`;
const MAX_DATA_POINTS = 50; 

function formatRuntime(totalSeconds: number): string {
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  return `${hours} h ${minutes} min`;
}

export default function Home() {

  const [vibrationDataX, setVibrationDataX] = useState<number[]>([]);
  const [vibrationDataY, setVibrationDataY] = useState<number[]>([]);
  const [vibrationDataZ, setVibrationDataZ] = useState<number[]>([]);
  const [labels, setLabels] = useState<string[]>([]);
  const [currentTemp, setCurrentTemp] = useState<number>(0);

  const [fftFreqs, setFftFreqs] = useState<string[]>([]); 
  const [fftMags, setFftMags] = useState<number[]>([]); 
  
  const [runtimeString, setRuntimeString] = useState<string>("0 h 0 min");
  
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const ws = useRef<WebSocket | null>(null);

  useEffect(() => {
    const connect = () => {
      ws.current = new WebSocket(ESP32_WS_URL);
      console.log("Tentando conectar ao WebSocket...");

      ws.current.onopen = () => {
        console.log("WebSocket Conectado!");
        setIsConnected(true);
        setError(null);
      };

      ws.current.onclose = () => {
        console.log("WebSocket Desconectado. Tentando reconectar em 3s...");
        setIsConnected(false);
        setError("Desconectado. Tentando reconectar...");
        setTimeout(connect, 3000); 
      };

      ws.current.onerror = (err) => {
        console.error("Erro no WebSocket:", err);
        setError("Erro de conexao. Verifique o IP e a rede.");
        ws.current?.close();
      };

      ws.current.onmessage = (event) => {
        const data = JSON.parse(event.data);

        switch (data.type) {
          
          case "axes":
            const currentTime = new Date().toLocaleTimeString();
            setVibrationDataX((prev) => { const n = [...prev, data.ax]; if (n.length > MAX_DATA_POINTS) n.shift(); return n; });
            setVibrationDataY((prev) => { const n = [...prev, data.ay]; if (n.length > MAX_DATA_POINTS) n.shift(); return n; });
            setVibrationDataZ((prev) => { const n = [...prev, data.az]; if (n.length > MAX_DATA_POINTS) n.shift(); return n; });
            setLabels((prev) => { const n = [...prev, currentTime]; if (n.length > MAX_DATA_POINTS) n.shift(); return n; });
            setCurrentTemp(data.temp);
            break;
            
          case "fft":
            const formattedFreqs = data.freqs.map((f: number) => f.toFixed(1)); 
            setFftFreqs(formattedFreqs);
            setFftMags(data.mags);
            break;
            
          case "runtime":
            setRuntimeString(formatRuntime(data.seconds));
            break;
        }
      };
    };

    connect();

    return () => {
      ws.current?.close();
    };
  }, []); 

  const commonChartOptions = { scales: { y: { beginAtZero: false, ticks: { color: '#5D4037' }, grid: { color: '#A1887F' }, title: { display: true, text: 'Aceleração (m/s²)', color: '#5D4037', font: { size: 14 } } }, x: { ticks: { display: false }, grid: { display: false }, title: { display: true, text: `Tempo (Últimos ${MAX_DATA_POINTS / 10}s)`, color: '#8D6E63', font: { size: 14 } } } }, plugins: { legend: { display: false } }, animation: { duration: 0 }, maintainAspectRatio: false };
  const chartDataX = { labels: labels, datasets: [{ label: 'Vibração X', data: vibrationDataX, borderColor: '#8D6E63', backgroundColor: 'rgba(141, 110, 99, 0.2)', borderWidth: 2, fill: true, tension: 0.1 }], };
  const chartDataY = { labels: labels, datasets: [{ label: 'Vibração Y', data: vibrationDataY, borderColor: '#BCAAA4', backgroundColor: 'rgba(188, 170, 164, 0.2)', borderWidth: 2, fill: true, tension: 0.1 }], };
  const chartDataZ = { labels: labels, datasets: [{ label: 'Vibração Z', data: vibrationDataZ, borderColor: '#5D4037', backgroundColor: 'rgba(93, 64, 55, 0.2)', borderWidth: 2, fill: true, tension: 0.1 }], };
  const chartDataFFT = { labels: fftFreqs, datasets: [{ label: 'Magnitude da Frequencia', data: fftMags,  backgroundColor: 'rgba(93, 64, 55, 0.6)', borderColor: '#5D4037', borderWidth: 1 }] };
  const chartOptionsFFT = { scales: { y: { beginAtZero: true, ticks: { color: '#5D4037' }, grid: { color: '#A1887F' }, title: { display: true, text: 'Magnitude', color: '#5D4037', font: { size: 14 } } }, x: { ticks: { color: '#8D6E63', maxTicksLimit: 10 }, grid: { display: false }, title: { display: true, text: 'Frequência (Hz)', color: '#8D6E63', font: { size: 14 } } } }, plugins: { legend: { display: false } }, animation: { duration: 250 }, maintainAspectRatio: false };

  return (
    <main className="flex min-h-screen flex-col items-center p-6 bg-amber-50 text-amber-900">
      
      <div className={`w-full max-w-6xl p-2 text-center mb-4 rounded-lg text-white font-semibold ${isConnected ? 'bg-green-600' : 'bg-red-600'}`}>
        {isConnected ? 'CONECTADO' : `DESCONECTADO (${error || '...'})`}
      </div>

      <h1 className="text-4xl font-bold mb-8 text-amber-950">Monitor de Vibracao em Tempo Real</h1>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-8 w-full max-w-4xl">
          <div className="bg-white p-4 rounded-lg shadow-md border border-amber-200 text-center">
              <h2 className="text-xl mb-1 text-amber-800">Horas de Uso</h2>
              <p className="text-4xl font-semibold text-amber-700">{runtimeString}</p>
          </div>
          <div className="bg-white p-4 rounded-lg shadow-md border border-amber-200 text-center">
              <h2 className="text-xl mb-1 text-amber-800">Temperatura do Sensor</h2>
              <p className="text-4xl font-semibold text-amber-700">
                  {currentTemp.toFixed(1)} <span className="text-lg">°C</span>
              </p>
          </div>
      </div>
      
      <div className="w-full max-w-6xl h-96 bg-white p-4 rounded-lg shadow-md border border-amber-200 mb-8">
        <h2 className="text-2xl font-semibold text-center mb-2 text-amber-800">Espectro de Frequencia (FFT)</h2>
        <div className="h-80">
          <Bar data={chartDataFFT} options={chartOptionsFFT} />
        </div>
      </div>
      
      <div className="grid grid-cols-1 gap-8 w-full max-w-6xl">
        
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