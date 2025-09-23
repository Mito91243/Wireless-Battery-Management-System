import React, { useState, useEffect } from 'react';
import { Battery, Zap, AlertTriangle, Thermometer, Activity, History, Clock, TrendingUp, AlertCircle, ExternalLink, Wifi, Home, BarChart3 } from 'lucide-react';
import { LineChart, Line, XAxis, YAxis, ResponsiveContainer, Legend } from 'recharts';
// Simple tabs implementation since shadcn/ui tabs are not available
const Tabs = ({ children, defaultValue, className }) => {
  const [activeTab, setActiveTab] = React.useState(defaultValue);
  return (
    <div className={className}>
      {React.Children.map(children, child => 
        React.cloneElement(child, { activeTab, setActiveTab })
      )}
    </div>
  );
};

const TabsList = ({ children, className, activeTab, setActiveTab }) => (
  <div className={`flex bg-gray-100 rounded-lg p-1 ${className}`}>
    {React.Children.map(children, child => 
      React.cloneElement(child, { activeTab, setActiveTab })
    )}
  </div>
);

const TabsTrigger = ({ value, children, className, activeTab, setActiveTab }) => (
  <button
    onClick={() => setActiveTab(value)}
    className={`flex-1 px-4 py-2 text-sm font-medium rounded-md transition-all ${
      activeTab === value 
        ? 'bg-white text-gray-900 shadow-sm' 
        : 'text-gray-600 hover:text-gray-900 hover:bg-gray-50'
    } ${className}`}
  >
    {children}
  </button>
);

const TabsContent = ({ value, children, className, activeTab }) => 
  activeTab === value ? <div className={className}>{children}</div> : null;

const Dashboard = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [isConnected, setIsConnected] = useState(true);

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Sample data for the chart
  const chartData = [
    { time: '09:28 PM', voltage: 45, temperature: 30, current: 8 },
    { time: '10:28 PM', voltage: 47, temperature: 29, current: 10 },
    { time: '11:28 PM', voltage: 48, temperature: 36, current: 8 },
    { time: '12:28 AM', voltage: 46, temperature: 30, current: 12 },
    { time: '01:28 AM', voltage: 49, temperature: 31, current: 6 },
    { time: '02:28 AM', voltage: 47, temperature: 38, current: 4 },
    { time: '03:28 AM', voltage: 45, temperature: 35, current: 14 },
    { time: '04:28 AM', voltage: 43, temperature: 39, current: 8 },
    { time: '05:28 AM', voltage: 45, temperature: 34, current: 6 },
    { time: '06:28 AM', voltage: 46, temperature: 36, current: 10 },
    { time: '07:28 AM', voltage: 47, temperature: 32, current: 12 },
    { time: '08:28 AM', voltage: 45, temperature: 38, current: 8 },
  ];

  const StatusBadge = ({ status }) => {
    const getStatusStyles = (status) => {
      switch (status) {
        case 'safe': return 'bg-green-100 text-green-700 border-green-200';
        case 'caution': return 'bg-yellow-100 text-yellow-700 border-yellow-200';
        case 'alert': return 'bg-red-100 text-red-700 border-red-200';
        default: return 'bg-gray-100 text-gray-700 border-gray-200';
      }
    };

    return (
      <span className={`px-2 py-1 rounded-full text-xs font-medium border ${getStatusStyles(status)}`}>
        {status.charAt(0).toUpperCase() + status.slice(1)}
      </span>
    );
  };

  const BatteryGrid = ({ cells }) => {
    return (
      <div className="space-y-4">
        <div className="flex items-center justify-between text-xs text-gray-600">
          <span>Battery Configuration: 13S4P (52 cells)</span>
          <span>13 series × 4 parallel</span>
        </div>
        <div className="grid gap-2" style={{gridTemplateColumns: 'repeat(13, minmax(0, 1fr))'}}>
          {cells.map((cell, index) => (
            <div
              key={index}
              className={`w-7 h-10 rounded-lg flex items-center justify-center transition-all duration-300 hover:scale-105 cursor-pointer shadow-sm hover:shadow-md ${
                cell.status === 'safe' 
                  ? 'bg-gradient-to-b from-emerald-400 to-emerald-500 hover:from-emerald-500 hover:to-emerald-600' : 
                cell.status === 'caution' 
                  ? 'bg-gradient-to-b from-amber-400 to-amber-500 hover:from-amber-500 hover:to-amber-600' : 
                  'bg-gradient-to-b from-red-400 to-red-500 hover:from-red-500 hover:to-red-600'
              }`}
              title={`Cell ${index + 1}: ${cell.value}V - Status: ${cell.status}`}
            >
              <span className="text-xs font-bold text-white leading-none">
                {cell.value}
              </span>
            </div>
          ))}
        </div>
        <div className="flex items-center gap-6 text-xs">
          <div className="flex items-center gap-2">
            <div className="w-3 h-3 bg-gradient-to-b from-emerald-400 to-emerald-500 rounded border border-emerald-600"></div>
            <span className="text-gray-600 font-medium">Safe ({cells.filter(c => c.status === 'safe').length})</span>
          </div>
          <div className="flex items-center gap-2">
            <div className="w-3 h-3 bg-gradient-to-b from-amber-400 to-amber-500 rounded border border-amber-600"></div>
            <span className="text-gray-600 font-medium">Caution ({cells.filter(c => c.status === 'caution').length})</span>
          </div>
          <div className="flex items-center gap-2">
            <div className="w-3 h-3 bg-gradient-to-b from-red-400 to-red-500 rounded border border-red-600"></div>
            <span className="text-gray-600 font-medium">Alert ({cells.filter(c => c.status === 'alert').length})</span>
          </div>
        </div>
      </div>
    );
  };

  const HistoryTable = ({ batteryPacks }) => {
    return (
      <div className="bg-white rounded-lg p-6 shadow-sm border border-gray-200">
        <div className="flex items-center justify-between mb-4">
          <h3 className="text-lg font-semibold text-gray-900">Battery Pack History</h3>
          <p className="text-sm text-gray-500">Click on any row to view detailed history</p>
        </div>
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead>
              <tr className="border-b border-gray-200">
                <th className="text-left py-3 px-4 font-medium text-gray-900">Pack ID</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Name</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">SOC (%)</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">SOH (%)</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Voltage (V)</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Current (A)</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Temperature (°C)</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Status</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Last Update</th>
                <th className="text-right py-3 px-4 font-medium text-gray-900">Action</th>
              </tr>
            </thead>
            <tbody>
              {batteryPacks.map((pack) => (
                <tr 
                  key={pack.id} 
                  className="border-b border-gray-100 hover:bg-gray-50 cursor-pointer transition-colors group"
                >
                  <td className="py-3 px-4 font-medium text-gray-900">{pack.id}</td>
                  <td className="py-3 px-4 text-gray-600">{pack.name}</td>
                  <td className="py-3 px-4 text-gray-900">{pack.soc}%</td>
                  <td className="py-3 px-4 text-gray-900">{pack.soh}%</td>
                  <td className="py-3 px-4 text-gray-900">{pack.voltage}V</td>
                  <td className="py-3 px-4 text-gray-900">{pack.current}A</td>
                  <td className="py-3 px-4 text-gray-900">{pack.temp}°C</td>
                  <td className="py-3 px-4">
                    <StatusBadge status={pack.status} />
                  </td>
                  <td className="py-3 px-4 text-gray-600 text-sm">
                    {currentTime.toLocaleTimeString()}
                  </td>
                  <td className="py-3 px-4 text-right">
                    <ExternalLink className="h-4 w-4 text-gray-400 group-hover:text-blue-600 transition-colors inline" />
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    );
  };

  const StatCard = ({ title, value, icon: Icon, color, subtitle }) => (
    <div className="bg-white rounded-lg p-6 shadow-sm border border-gray-200">
      <div className="flex items-center justify-between">
        <div>
          <p className="text-gray-500 text-sm font-medium">{title}</p>
          <p className="text-2xl font-bold text-gray-900 mt-1">{value}</p>
          {subtitle && <p className="text-xs text-gray-400 mt-1">{subtitle}</p>}
        </div>
        <div className={`p-3 rounded-lg ${color}`}>
          <Icon className="w-6 h-6 text-white" />
        </div>
      </div>
    </div>
  );

  const BatteryPackCard = ({ pack }) => {
    const getStatusGradient = (status) => {
      switch (status) {
        case 'safe': return 'from-green-50 to-green-25 border-green-200';
        case 'caution': return 'from-yellow-50 to-yellow-25 border-yellow-200';
        case 'alert': return 'from-red-50 to-red-25 border-red-200';
        default: return 'from-gray-50 to-white border-gray-200';
      }
    };

    const getStatusIcon = (status) => {
      switch (status) {
        case 'safe': return <TrendingUp className="h-4 w-4 text-green-600" />;
        case 'caution': return <AlertCircle className="h-4 w-4 text-yellow-600" />;
        case 'alert': return <AlertCircle className="h-4 w-4 text-red-600" />;
        default: return <Battery className="h-4 w-4 text-gray-600" />;
      }
    };

    return (
      <div className={`rounded-lg p-6 shadow-sm border-2 bg-gradient-to-br ${getStatusGradient(pack.status)} hover:shadow-md transition-all duration-300 hover:scale-[1.01] group relative overflow-hidden`}>
        {/* Status Indicator Bar */}
        <div className={`absolute top-0 left-0 right-0 h-1 ${
          pack.status === 'safe' ? 'bg-gradient-to-r from-green-500 to-green-400' :
          pack.status === 'caution' ? 'bg-gradient-to-r from-yellow-500 to-yellow-400' :
          'bg-gradient-to-r from-red-500 to-red-400'
        }`} />

        {/* Header */}
        <div className="flex items-center justify-between mb-6 mt-2">
          <div className="flex items-center gap-3">
            <div className={`p-3 rounded-xl shadow-lg ${
              pack.status === 'safe' ? 'bg-gradient-to-br from-green-500 to-green-400' :
              pack.status === 'caution' ? 'bg-gradient-to-br from-yellow-500 to-yellow-400' :
              'bg-gradient-to-br from-red-500 to-red-400'
            }`}>
              <Battery className="h-6 w-6 text-white" />
            </div>
            <div>
              <h3 className="text-lg font-bold text-gray-900">{pack.name}</h3>
              <p className="text-sm text-gray-500 font-mono">{pack.id}</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            {getStatusIcon(pack.status)}
            <StatusBadge status={pack.status} />
          </div>
        </div>

        {/* Key Metrics */}
        <div className="grid grid-cols-2 gap-4 mb-6">
          <div className="space-y-2">
            <div className="flex items-center gap-2">
              <Battery className="h-4 w-4 text-gray-500" />
              <span className="text-sm font-medium text-gray-600">State of Charge</span>
            </div>
            <div className="text-3xl font-bold text-gray-900">{pack.soc}%</div>
            <div className="w-full bg-gray-200 rounded-full h-3">
              <div 
                className="bg-blue-500 h-3 rounded-full transition-all duration-300" 
                style={{ width: `${pack.soc}%` }}
              ></div>
            </div>
          </div>
          <div className="space-y-2">
            <div className="flex items-center gap-2">
              <TrendingUp className="h-4 w-4 text-gray-500" />
              <span className="text-sm font-medium text-gray-600">State of Health</span>
            </div>
            <div className="text-3xl font-bold text-gray-900">{pack.soh}%</div>
            <div className="w-full bg-gray-200 rounded-full h-3">
              <div 
                className="bg-blue-500 h-3 rounded-full transition-all duration-300" 
                style={{ width: `${pack.soh}%` }}
              ></div>
            </div>
          </div>
        </div>

        {/* Electrical Properties */}
        <div className="grid grid-cols-3 gap-4 p-4 bg-white/50 rounded-xl border border-gray-200 mb-6">
          <div className="text-center space-y-1">
            <Zap className="h-5 w-5 text-blue-500 mx-auto" />
            <div className="text-lg font-bold text-gray-900">{pack.voltage}</div>
            <div className="text-xs text-gray-500 font-medium">Voltage (V)</div>
          </div>
          <div className="text-center space-y-1">
            <Activity className="h-5 w-5 text-green-500 mx-auto" />
            <div className="text-lg font-bold text-gray-900">{pack.current}</div>
            <div className="text-xs text-gray-500 font-medium">Current (A)</div>
          </div>
          <div className="text-center space-y-1">
            <Thermometer className="h-5 w-5 text-orange-500 mx-auto" />
            <div className="text-lg font-bold text-gray-900">{pack.temp}</div>
            <div className="text-xs text-gray-500 font-medium">Temp (°C)</div>
          </div>
        </div>

        {/* Battery Grid */}
        <div className="mb-6">
          <BatteryGrid cells={pack.cells} />
        </div>

        {/* Footer */}
        <div className="flex items-center justify-between pt-2 border-t border-gray-200">
          <div className="flex items-center gap-2 text-xs text-gray-500">
            <Clock className="h-3 w-3" />
            <span>Updated: {currentTime.toLocaleTimeString()}</span>
          </div>
        </div>
      </div>
    );
  };

  const TrendChart = ({ data, title }) => (
    <div className="bg-white rounded-lg p-6 shadow-sm border border-gray-200">
      <div className="mb-6">
        <h2 className="text-lg font-semibold text-gray-900 mb-2">{title}</h2>
        <p className="text-sm text-gray-500">Real-time monitoring data visualization</p>
      </div>

      <div className="h-64">
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={data}>
            <XAxis 
              dataKey="time" 
              axisLine={false}
              tickLine={false}
              tick={{ fontSize: 12, fill: '#6B7280' }}
            />
            <YAxis 
              axisLine={false}
              tickLine={false}
              tick={{ fontSize: 12, fill: '#6B7280' }}
            />
            <Legend />
            <Line 
              type="monotone" 
              dataKey="voltage" 
              stroke="#3B82F6" 
              strokeWidth={2}
              name="Voltage (V)"
              dot={false}
            />
            <Line 
              type="monotone" 
              dataKey="temperature" 
              stroke="#F59E0B" 
              strokeWidth={2}
              name="Temperature (°C)"
              dot={false}
            />
            <Line 
              type="monotone" 
              dataKey="current" 
              stroke="#10B981" 
              strokeWidth={2}
              name="Current (A)"
              dot={false}
            />
          </LineChart>
        </ResponsiveContainer>
      </div>
    </div>
  );

  // Sample battery pack data
  const batteryPacks = [
    {
      name: 'Pack Alpha',
      id: 'BP001',
      status: 'safe',
      soc: 85,
      soh: 98,
      voltage: '48.10',
      current: '12.84',
      temp: '29.07',
      config: '13S4P (52 cells)',
      series: '13',
      parallel: '4',
      cells: Array.from({ length: 52 }, (_, i) => ({
        value: (3.6 + Math.random() * 0.4).toFixed(1),
        status: Math.random() > 0.9 ? 'caution' : 'safe'
      }))
    },
    {
      name: 'Pack Beta',
      id: 'BP002',
      status: 'caution',
      soc: 62,
      soh: 95,
      voltage: '46.80',
      current: '8.62',
      temp: '31.03',
      config: '13S4P (52 cells)',
      series: '13',
      parallel: '4',
      cells: Array.from({ length: 52 }, (_, i) => ({
        value: (3.5 + Math.random() * 0.5).toFixed(1),
        status: Math.random() > 0.85 ? 'caution' : 'safe'
      }))
    }
  ];

  const DashboardOverview = ({ batteryPacks }) => (
    <div className="space-y-8">
      {/* Stats Cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <StatCard
          title="Total Power"
          value="1.97 kW"
          icon={Zap}
          color="bg-blue-500"
        />
        <StatCard
          title="Total Current"
          value="20.80 A"
          icon={Activity}
          color="bg-green-500"
        />
        <StatCard
          title="Active Packs"
          value="2"
          icon={Battery}
          color="bg-blue-500"
        />
        <StatCard
          title="Active Alerts"
          value="1"
          icon={AlertTriangle}
          color="bg-yellow-500"
          subtitle="0 critical, 1 warnings"
        />
      </div>

      {/* System Performance Overview */}
      <TrendChart data={chartData} title="System Performance Overview" />

      {/* Battery Pack Status */}
      <div className="mb-6">
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-lg font-semibold text-gray-900">Battery Pack Status</h2>
          <div className="flex items-center gap-4 text-sm">
            <div className="flex items-center gap-2">
              <div className="w-3 h-3 bg-green-500 rounded-full"></div>
              <span className="text-gray-600">Operational (1)</span>
            </div>
            <div className="flex items-center gap-2">
              <div className="w-3 h-3 bg-yellow-500 rounded-full"></div>
              <span className="text-gray-600">Caution (1)</span>
            </div>
            <div className="flex items-center gap-2">
              <div className="w-3 h-3 bg-red-500 rounded-full"></div>
              <span className="text-gray-600">Critical (0)</span>
            </div>
          </div>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {batteryPacks.map((pack, index) => (
            <BatteryPackCard key={index} pack={pack} />
          ))}
        </div>
      </div>
    </div>
  );

  return (
    <div className="min-h-screen bg-gray-50 p-6">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="flex items-center justify-between mb-8">
          <div className="flex items-center gap-4">
            <div className="h-12 w-12 bg-gradient-to-br from-blue-500 to-blue-600 rounded-xl flex items-center justify-center">
              <Battery className="h-6 w-6 text-white" />
            </div>
            <div>
              <h1 className="text-3xl font-bold text-gray-900">
                Battery Management System
              </h1>
              <p className="text-gray-600">
                Real-time monitoring of wireless battery packs via ESP-NOW
              </p>
            </div>
          </div>
          
          <div className="flex items-center gap-2">
            <div className={`h-3 w-3 rounded-full ${isConnected ? 'bg-green-500 animate-pulse' : 'bg-red-500'}`} />
            <Wifi className="h-5 w-5 text-gray-600" />
            <span className="text-sm text-gray-600">
              {isConnected ? 'Connected' : 'Disconnected'}
            </span>
          </div>
        </div>

        {/* Dashboard Tabs */}
        <Tabs defaultValue="summary" className="w-full">
          <TabsList className="grid w-full grid-cols-3">
            <TabsTrigger value="summary" className="flex justify-center items-center gap-2">
              <Home className="h-4 w-4" />
              Summary & Stats
            </TabsTrigger>
            <TabsTrigger value="charts" className="flex justify-center items-center gap-2">
              <BarChart3 className="h-4 w-4" />
              Charts
            </TabsTrigger>
            <TabsTrigger value="history" className="flex justify-center items-center gap-2">
              <History className="h-4 w-4" />
              History
            </TabsTrigger>
          </TabsList>
          
          <TabsContent value="summary" className="space-y-6">
            <DashboardOverview batteryPacks={batteryPacks} />
          </TabsContent>
          
          <TabsContent value="charts" className="space-y-8">
            <div className="space-y-8">
              <TrendChart data={chartData} title="Voltage & Temperature Trends (24h)" />
              <TrendChart data={chartData} title="Current & Power Trends (24h)" />
              <TrendChart data={chartData} title="System Efficiency Overview (24h)" />
              <TrendChart data={chartData} title="Battery Health Monitoring (24h)" />
            </div>
          </TabsContent>
          
          <TabsContent value="history">
            <HistoryTable batteryPacks={batteryPacks} />
          </TabsContent>
        </Tabs>
      </div>
    </div>
  );
};

export default Dashboard;