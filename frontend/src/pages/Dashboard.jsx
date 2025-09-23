import React, { useState, useEffect, useMemo, useCallback, useRef } from 'react';
import { Battery, Zap, AlertTriangle, Thermometer, Activity, History, Clock, TrendingUp, AlertCircle, ExternalLink, Wifi, Home, BarChart3, RefreshCw } from 'lucide-react';
import { LineChart, Line, XAxis, YAxis, ResponsiveContainer, Legend, Tooltip, CartesianGrid } from 'recharts';

// Custom Tabs Component
const TabsContext = React.createContext();

const Tabs = ({ children, defaultValue, className }) => {
  const [activeTab, setActiveTab] = useState(defaultValue);
  return (
    <TabsContext.Provider value={{ activeTab, setActiveTab }}>
      <div className={className}>{children}</div>
    </TabsContext.Provider>
  );
};

const TabsList = ({ children, className }) => (
  <div className={`flex bg-gray-100 rounded-lg p-1 ${className}`}>{children}</div>
);

const TabsTrigger = ({ value, children, className }) => {
  const { activeTab, setActiveTab } = React.useContext(TabsContext);
  return (
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
};

const TabsContent = ({ value, children, className }) => {
  const { activeTab } = React.useContext(TabsContext);
  return activeTab === value ? <div className={className}>{children}</div> : null;
};

// Utility Components
const StatusBadge = ({ status }) => {
  const styles = {
    safe: 'bg-green-100 text-green-700 border-green-200',
    caution: 'bg-yellow-100 text-yellow-700 border-yellow-200',
    alert: 'bg-red-100 text-red-700 border-red-200',
    default: 'bg-gray-100 text-gray-700 border-gray-200'
  };

  return (
    <span className={`px-2 py-1 rounded-full text-xs font-medium border ${styles[status] || styles.default}`}>
      {status.charAt(0).toUpperCase() + status.slice(1)}
    </span>
  );
};

const LoadingSpinner = () => (
  <div className="flex items-center justify-center p-8">
    <RefreshCw className="h-8 w-8 text-blue-500 animate-spin" />
  </div>
);

const ErrorMessage = ({ message }) => (
  <div className="bg-red-50 border border-red-200 rounded-lg p-4 text-red-700">
    <AlertCircle className="h-5 w-5 inline mr-2" />
    {message}
  </div>
);

// Battery Grid Component
const BatteryGrid = ({ cells }) => {
  const cellStats = useMemo(() => ({
    safe: cells.filter(c => c.status === 'safe').length,
    caution: cells.filter(c => c.status === 'caution').length,
    alert: cells.filter(c => c.status === 'alert').length
  }), [cells]);

  const getCellColor = (status) => {
    switch(status) {
      case 'safe': return 'from-emerald-400 to-emerald-500 hover:from-emerald-500 hover:to-emerald-600';
      case 'caution': return 'from-amber-400 to-amber-500 hover:from-amber-500 hover:to-amber-600';
      case 'alert': return 'from-red-400 to-red-500 hover:from-red-500 hover:to-red-600';
      default: return 'from-gray-400 to-gray-500';
    }
  };

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
            className={`w-7 h-10 rounded-lg flex items-center justify-center transition-all duration-300 hover:scale-105 cursor-pointer shadow-sm hover:shadow-md bg-gradient-to-b ${getCellColor(cell.status)}`}
            title={`Cell ${index + 1}: ${cell.value}V - Status: ${cell.status}`}
          >
            <span className="text-xs font-bold text-white leading-none">
              {cell.value}
            </span>
          </div>
        ))}
      </div>
      <div className="flex items-center gap-6 text-xs">
        {Object.entries(cellStats).map(([status, count]) => (
          <div key={status} className="flex items-center gap-2">
            <div className={`w-3 h-3 rounded border bg-gradient-to-b ${getCellColor(status)}`}></div>
            <span className="text-gray-600 font-medium">
              {status.charAt(0).toUpperCase() + status.slice(1)} ({count})
            </span>
          </div>
        ))}
      </div>
    </div>
  );
};

// Enhanced Chart Component - Memoized to prevent unnecessary re-renders
const TrendChart = React.memo(({ data, title, subtitle, dataKeys }) => (
  <div className="bg-white rounded-lg p-6 shadow-sm border border-gray-200">
    <div className="mb-6">
      <h2 className="text-lg font-semibold text-gray-900 mb-2">{title}</h2>
      {subtitle && <p className="text-sm text-gray-500">{subtitle}</p>}
    </div>

    <div className="h-64">
      {data.length > 0 ? (
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={data} margin={{ top: 5, right: 30, left: 20, bottom: 5 }}>
            <CartesianGrid strokeDasharray="3 3" stroke="#E5E7EB" />
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
            <Tooltip 
              contentStyle={{ 
                backgroundColor: 'rgba(255, 255, 255, 0.95)', 
                border: '1px solid #E5E7EB',
                borderRadius: '8px'
              }}
            />
            <Legend />
            {dataKeys.map(({ key, color, name }) => (
              <Line 
                key={key}
                type="monotone" 
                dataKey={key} 
                stroke={color} 
                strokeWidth={2}
                name={name}
                dot={false}
                activeDot={{ r: 6 }}
                connectNulls={false}
                isAnimationActive={false}
              />
            ))}
          </LineChart>
        </ResponsiveContainer>
      ) : (
        <div className="flex items-center justify-center h-full text-gray-400">
          <p>No data available yet. Data will appear as it's collected.</p>
        </div>
      )}
    </div>
  </div>
));

// Main Dashboard Component
const Dashboard = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [isConnected, setIsConnected] = useState(true);
  const [batteryPacks, setBatteryPacks] = useState([]);
  const [historicalData, setHistoricalData] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [refreshKey, setRefreshKey] = useState(0);
  
  // Add this ref to track last update time
  const lastUpdateTime = useRef(null);

  // Fetch battery data
  const fetchBatteryData = useCallback(async () => {
    try {
      setError(null);
      const response = await fetch("http://127.0.0.1:8000/v1/pack-data/latest");
      
      if (!response.ok) {
        throw new Error(`Failed to fetch: ${response.status}`);
      }
      
      const data = await response.json();
      setBatteryPacks(data.packs || []);
      setIsConnected(true);
      
      // Update historical data with new data point
      const now = new Date();
      const timestamp = now.toLocaleTimeString('en-US', { 
        hour: '2-digit', 
        minute: '2-digit',
        second: '2-digit',
        hour12: true 
      });

      if (data.packs && data.packs.length > 0) {
        // Only update if at least 1 second has passed
        if (!lastUpdateTime.current || now.getTime() - lastUpdateTime.current >= 1000) {
          lastUpdateTime.current = now.getTime();
          
          const avgVoltage = data.packs.reduce((sum, p) => sum + parseFloat(p.voltage), 0) / data.packs.length;
          const avgCurrent = data.packs.reduce((sum, p) => sum + parseFloat(p.current), 0) / data.packs.length;
          const avgTemp = data.packs.reduce((sum, p) => sum + parseFloat(p.temp), 0) / data.packs.length;
          
          const newDataPoint = {
            time: timestamp,
            voltage: Number(avgVoltage.toFixed(1)),
            current: Number(avgCurrent.toFixed(1)),
            temperature: Number(avgTemp.toFixed(1)),
            power: Number((avgVoltage * avgCurrent).toFixed(1))
          };
          
          setHistoricalData(prev => {
            const newData = [...prev, newDataPoint];
            // Keep only last 50 data points for better visibility
            return newData.slice(-50);
          });
        }
      }
      
      setLoading(false);
    } catch (error) {
      console.error("Error fetching battery data:", error);
      setError(error.message);
      setIsConnected(false);
      setLoading(false);
    }
  }, []);

  // Update current time
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Fetch data on mount and set up polling
  useEffect(() => {
    fetchBatteryData();
    const interval = setInterval(fetchBatteryData, 3000);
    return () => clearInterval(interval);
  }, [fetchBatteryData, refreshKey]);

  // Calculate aggregate statistics
  const stats = useMemo(() => {
    if (!batteryPacks.length) {
      return {
        totalPower: 0,
        totalCurrent: 0,
        activePacks: 0,
        alerts: { critical: 0, warnings: 0 },
        avgSoc: 0,
        avgSoh: 0
      };
    }

    const totalVoltage = batteryPacks.reduce((sum, p) => sum + parseFloat(p.voltage), 0);
    const totalCurrent = batteryPacks.reduce((sum, p) => sum + parseFloat(p.current), 0);
    const totalPower = (totalVoltage * totalCurrent / batteryPacks.length) / 1000; // kW
    
    const alerts = batteryPacks.reduce((acc, p) => {
      if (p.status === 'alert') acc.critical++;
      if (p.status === 'caution') acc.warnings++;
      return acc;
    }, { critical: 0, warnings: 0 });

    return {
      totalPower: totalPower.toFixed(2),
      totalCurrent: totalCurrent.toFixed(2),
      activePacks: batteryPacks.length,
      alerts,
      avgSoc: (batteryPacks.reduce((sum, p) => sum + p.soc, 0) / batteryPacks.length).toFixed(0),
      avgSoh: (batteryPacks.reduce((sum, p) => sum + p.soh, 0) / batteryPacks.length).toFixed(0)
    };
  }, [batteryPacks]);

  // Stat Card Component
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

  // Battery Pack Card Component
  const BatteryPackCard = ({ pack }) => {
    const getStatusGradient = (status) => ({
      safe: 'from-green-50 to-green-25 border-green-200',
      caution: 'from-yellow-50 to-yellow-25 border-yellow-200',
      alert: 'from-red-50 to-red-25 border-red-200'
    }[status] || 'from-gray-50 to-white border-gray-200');

    const getStatusBarColor = (status) => ({
      safe: 'bg-gradient-to-r from-green-500 to-green-400',
      caution: 'bg-gradient-to-r from-yellow-500 to-yellow-400',
      alert: 'bg-gradient-to-r from-red-500 to-red-400'
    }[status] || 'bg-gray-400');

    return (
      <div className={`rounded-lg p-6 shadow-sm border-2 bg-gradient-to-br ${getStatusGradient(pack.status)} hover:shadow-md transition-all duration-300 hover:scale-[1.01] relative overflow-hidden`}>
        <div className={`absolute top-0 left-0 right-0 h-1 ${getStatusBarColor(pack.status)}`} />
        
        {/* Header */}
        <div className="flex items-center justify-between mb-6 mt-2">
          <div className="flex items-center gap-3">
            <div className={`p-3 rounded-xl shadow-lg ${getStatusBarColor(pack.status)}`}>
              <Battery className="h-6 w-6 text-white" />
            </div>
            <div>
              <h3 className="text-lg font-bold text-gray-900">{pack.name}</h3>
              <p className="text-sm text-gray-500 font-mono">{pack.id}</p>
            </div>
          </div>
          <StatusBadge status={pack.status} />
        </div>

        {/* Metrics Grid */}
        <div className="grid grid-cols-2 gap-4 mb-6">
          {[
            { label: 'State of Charge', value: pack.soc, icon: Battery },
            { label: 'State of Health', value: pack.soh, icon: TrendingUp }
          ].map(({ label, value, icon: Icon }) => (
            <div key={label} className="space-y-2">
              <div className="flex items-center gap-2">
                <Icon className="h-4 w-4 text-gray-500" />
                <span className="text-sm font-medium text-gray-600">{label}</span>
              </div>
              <div className="text-3xl font-bold text-gray-900">{value}%</div>
              <div className="w-full bg-gray-200 rounded-full h-3">
                <div 
                  className="bg-blue-500 h-3 rounded-full transition-all duration-300" 
                  style={{ width: `${value}%` }}
                />
              </div>
            </div>
          ))}
        </div>

        {/* Electrical Properties */}
        <div className="grid grid-cols-3 gap-4 p-4 bg-white/50 rounded-xl border border-gray-200 mb-6">
          {[
            { label: 'Voltage', value: pack.voltage, unit: 'V', icon: Zap, color: 'text-blue-500' },
            { label: 'Current', value: pack.current, unit: 'A', icon: Activity, color: 'text-green-500' },
            { label: 'Temp', value: pack.temp, unit: '°C', icon: Thermometer, color: 'text-orange-500' }
          ].map(({ label, value, unit, icon: Icon, color }) => (
            <div key={label} className="text-center space-y-1">
              <Icon className={`h-5 w-5 ${color} mx-auto`} />
              <div className="text-lg font-bold text-gray-900">{value}</div>
              <div className="text-xs text-gray-500 font-medium">{label} ({unit})</div>
            </div>
          ))}
        </div>

        <BatteryGrid cells={pack.cells} />

        {/* Footer */}
        <div className="flex items-center justify-between pt-4 mt-4 border-t border-gray-200">
          <div className="flex items-center gap-2 text-xs text-gray-500">
            <Clock className="h-3 w-3" />
            <span>Updated: {currentTime.toLocaleTimeString('en-US', { 
              hour: '2-digit', 
              minute: '2-digit', 
              second: '2-digit',
              hour12: true 
            })}</span>
          </div>
        </div>
      </div>
    );
  };

  // History Table Component
  const HistoryTable = ({ packs }) => (
    <div className="bg-white rounded-lg p-6 shadow-sm border border-gray-200">
      <div className="flex items-center justify-between mb-4">
        <h3 className="text-lg font-semibold text-gray-900">Battery Pack History</h3>
        <button 
          onClick={() => setRefreshKey(k => k + 1)}
          className="flex items-center gap-2 px-3 py-1.5 text-sm text-blue-600 hover:bg-blue-50 rounded-lg transition-colors"
        >
          <RefreshCw className="h-4 w-4" />
          Refresh
        </button>
      </div>
      
      {packs.length > 0 ? (
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead>
              <tr className="border-b border-gray-200">
                <th className="text-left py-3 px-4 font-medium text-gray-900">Pack ID</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Name</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">SOC</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">SOH</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Voltage</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Current</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Temp</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Status</th>
                <th className="text-left py-3 px-4 font-medium text-gray-900">Last Update</th>
              </tr>
            </thead>
            <tbody>
              {packs.map((pack) => (
                <tr 
                  key={pack.id} 
                  className="border-b border-gray-100 hover:bg-gray-50 transition-colors"
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
                    {currentTime.toLocaleTimeString('en-US', { 
                      hour: '2-digit', 
                      minute: '2-digit', 
                      second: '2-digit',
                      hour12: true 
                    })}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      ) : (
        <p className="text-center text-gray-500 py-8">No battery pack data available</p>
      )}
    </div>
  );

  // Dashboard Overview Component
  const DashboardOverview = () => (
    <div className="space-y-8">
      {/* Stats Cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <StatCard
          title="Total Power"
          value={`${stats.totalPower} kW`}
          icon={Zap}
          color="bg-blue-500"
        />
        <StatCard
          title="Total Current"
          value={`${stats.totalCurrent} A`}
          icon={Activity}
          color="bg-green-500"
        />
        <StatCard
          title="Active Packs"
          value={stats.activePacks}
          icon={Battery}
          color="bg-blue-500"
        />
        <StatCard
          title="Active Alerts"
          value={stats.alerts.critical + stats.alerts.warnings}
          icon={AlertTriangle}
          color={stats.alerts.critical > 0 ? "bg-red-500" : "bg-yellow-500"}
          subtitle={`${stats.alerts.critical} critical, ${stats.alerts.warnings} warnings`}
        />
      </div>

      {/* Real-time Chart */}
      <TrendChart 
        data={memoizedHistoricalData} 
        title="System Performance Overview" 
        subtitle="Real-time monitoring data from battery packs"
        dataKeys={[
          { key: 'voltage', color: '#3B82F6', name: 'Voltage (V)' },
          { key: 'temperature', color: '#F59E0B', name: 'Temperature (°C)' },
          { key: 'current', color: '#10B981', name: 'Current (A)' }
        ]}
      />

      {/* Battery Pack Cards */}
      <div>
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-lg font-semibold text-gray-900">Battery Pack Status</h2>
          <div className="flex items-center gap-4 text-sm">
            {['safe', 'caution', 'alert'].map((status) => {
              const count = batteryPacks.filter(p => p.status === status).length;
              const colors = {
                safe: 'bg-green-500',
                caution: 'bg-yellow-500',
                alert: 'bg-red-500'
              };
              return (
                <div key={status} className="flex items-center gap-2">
                  <div className={`w-3 h-3 ${colors[status]} rounded-full`}></div>
                  <span className="text-gray-600">
                    {status.charAt(0).toUpperCase() + status.slice(1)} ({count})
                  </span>
                </div>
              );
            })}
          </div>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {loading && <LoadingSpinner />}
          {error && <ErrorMessage message={error} />}
          {!loading && !error && batteryPacks.length > 0 && 
            batteryPacks.map((pack) => (
              <BatteryPackCard key={pack.id} pack={pack} />
            ))
          }
          {!loading && !error && batteryPacks.length === 0 && (
            <p className="text-gray-500 col-span-2 text-center py-8">
              No battery pack data available. Waiting for data...
            </p>
          )}
        </div>
      </div>
    </div>
  );

  // Memoize chart data to prevent unnecessary re-renders
  const memoizedHistoricalData = useMemo(() => historicalData, [historicalData]);

  return (
    <div className="min-h-screen bg-gray-50 p-6">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="flex items-center justify-between mb-8">
          <div className="flex items-center gap-4">
            <div className="h-12 w-12 bg-gradient-to-br from-blue-500 to-blue-600 rounded-xl flex items-center justify-center shadow-lg">
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
          
          <div className="flex items-center gap-4">
            <div className="flex items-center gap-2">
              <div className={`h-3 w-3 rounded-full ${isConnected ? 'bg-green-500 animate-pulse' : 'bg-red-500'}`} />
              <Wifi className="h-5 w-5 text-gray-600" />
              <span className="text-sm text-gray-600">
                {isConnected ? 'Connected' : 'Disconnected'}
              </span>
            </div>
            <button
              onClick={() => setRefreshKey(k => k + 1)}
              className="p-2 hover:bg-gray-100 rounded-lg transition-colors"
              title="Refresh data"
            >
              <RefreshCw className="h-5 w-5 text-gray-600" />
            </button>
          </div>
        </div>

        {/* Dashboard Tabs */}
        <Tabs defaultValue="summary" className="w-full">
          <TabsList className="grid w-full grid-cols-3 mb-6">
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
          
          <TabsContent value="summary">
            <DashboardOverview />
          </TabsContent>
          
          <TabsContent value="charts" className="space-y-8">
            <TrendChart 
              data={memoizedHistoricalData}
              title="Voltage & Current Trends"
              subtitle="Real-time electrical measurements"
              dataKeys={[
                { key: 'voltage', color: '#3B82F6', name: 'Voltage (V)' },
                { key: 'current', color: '#10B981', name: 'Current (A)' }
              ]}
            />
            <TrendChart 
              data={memoizedHistoricalData}
              title="Temperature Monitoring"
              subtitle="Battery pack temperature over time"
              dataKeys={[
                { key: 'temperature', color: '#F59E0B', name: 'Temperature (°C)' }
              ]}
            />
            <TrendChart 
              data={memoizedHistoricalData}
              title="Power Output"
              subtitle="Calculated power (V × A)"
              dataKeys={[
                { key: 'power', color: '#8B5CF6', name: 'Power (W)' }
              ]}
            />
          </TabsContent>
          
          <TabsContent value="history">
            <HistoryTable packs={batteryPacks} />
          </TabsContent>
        </Tabs>
      </div>
    </div>
  );
};

export default Dashboard;