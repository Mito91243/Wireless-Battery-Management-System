import React, { useState, useEffect, useMemo, useCallback, useRef } from 'react';
import { Battery, Zap, AlertTriangle, Thermometer, Activity, History, Clock, TrendingUp, AlertCircle, ExternalLink, Wifi, Home, BarChart3, RefreshCw, Plus, X, Trash2, LogOut } from 'lucide-react';
import { LineChart, Line, XAxis, YAxis, ResponsiveContainer, Legend, Tooltip, CartesianGrid } from 'recharts';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { apiFetch } from '../lib/api';

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
    safe: 'bg-emerald-50 text-emerald-600 border-emerald-200',
    caution: 'bg-amber-50 text-amber-600 border-amber-200',
    alert: 'bg-rose-50 text-rose-600 border-rose-200',
    default: 'bg-gray-50 text-gray-600 border-gray-200'
  };

  return (
    <span className={`px-2.5 py-1 rounded-md text-xs font-semibold border ${styles[status] || styles.default}`}>
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
const BatteryGrid = ({ cells, config, series, parallel }) => {
  const s = parseInt(series) || 13;
  const p = parseInt(parallel) || 4;

  const cellStats = useMemo(() => ({
    safe: cells.filter(c => c.status === 'safe').length,
    caution: cells.filter(c => c.status === 'caution').length,
    alert: cells.filter(c => c.status === 'alert').length
  }), [cells]);

  const getCellBg = (status) => {
    switch(status) {
      case 'safe': return 'bg-slate-100 text-slate-700 border-slate-200';
      case 'caution': return 'bg-amber-50 text-amber-700 border-amber-300';
      case 'alert': return 'bg-rose-50 text-rose-700 border-rose-300';
      default: return 'bg-gray-100 text-gray-500 border-gray-200';
    }
  };

  const getLegendDot = (status) => ({
    safe: 'bg-slate-400',
    caution: 'bg-amber-400',
    alert: 'bg-rose-400',
  }[status] || 'bg-gray-300');

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between text-xs text-gray-400 font-medium">
        <span>{config || `${s}S${p}P (${cells.length} cells)`}</span>
        <span>{s}S x {p}P</span>
      </div>
      <div className="grid gap-1" style={{gridTemplateColumns: `repeat(${s}, minmax(0, 1fr))`}}>
        {cells.map((cell, index) => (
          <div
            key={index}
            className={`h-8 rounded border flex items-center justify-center transition-colors cursor-default ${getCellBg(cell.status)}`}
            title={`Cell ${index + 1}: ${cell.value}V`}
          >
            <span className="text-[10px] font-mono font-semibold leading-none">
              {cell.value}
            </span>
          </div>
        ))}
      </div>
      <div className="flex items-center gap-4 text-xs text-gray-500">
        {Object.entries(cellStats).map(([status, count]) => (
          <div key={status} className="flex items-center gap-1.5">
            <div className={`w-2 h-2 rounded-full ${getLegendDot(status)}`}></div>
            <span>{status.charAt(0).toUpperCase() + status.slice(1)} ({count})</span>
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

// Add Pack Modal Component
const AddPackModal = ({ isOpen, onClose, onPackCreated }) => {
  const [form, setForm] = useState({ name: '', pack_identifier: '', series_count: 13, parallel_count: 4 });
  const [error, setError] = useState('');
  const [submitting, setSubmitting] = useState(false);

  if (!isOpen) return null;

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!form.name.trim() || !form.pack_identifier.trim()) {
      setError('Pack name and identifier are required');
      return;
    }
    setSubmitting(true);
    setError('');
    try {
      await apiFetch('/v1/packs', {
        method: 'POST',
        body: JSON.stringify({
          name: form.name,
          pack_identifier: form.pack_identifier,
          series_count: parseInt(form.series_count) || 13,
          parallel_count: parseInt(form.parallel_count) || 4,
        }),
      });
      setForm({ name: '', pack_identifier: '', series_count: 13, parallel_count: 4 });
      onPackCreated();
      onClose();
    } catch (err) {
      setError(err.message || 'Failed to create pack');
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <div className="fixed inset-0 bg-black/50 z-50 flex items-center justify-center p-4">
      <div className="bg-white rounded-xl shadow-2xl w-full max-w-md">
        <div className="flex items-center justify-between p-6 border-b border-gray-200">
          <h2 className="text-xl font-bold text-gray-900">Add Battery Pack</h2>
          <button onClick={onClose} className="p-1 hover:bg-gray-100 rounded-lg">
            <X className="h-5 w-5 text-gray-500" />
          </button>
        </div>
        <form onSubmit={handleSubmit} className="p-6 space-y-4">
          {error && (
            <div className="p-3 bg-red-50 border border-red-200 rounded-md text-sm text-red-600">{error}</div>
          )}
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">Pack Name</label>
            <input
              type="text"
              value={form.name}
              onChange={(e) => setForm(f => ({ ...f, name: e.target.value }))}
              placeholder="e.g. Pack Alpha"
              className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">Pack ID (Hardware Identifier)</label>
            <input
              type="text"
              value={form.pack_identifier}
              onChange={(e) => setForm(f => ({ ...f, pack_identifier: e.target.value }))}
              placeholder="e.g. BP001 or sender index"
              className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
            <p className="mt-1 text-xs text-gray-500">This should match the hardware sender ID of your battery pack</p>
          </div>
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">Series Count</label>
              <input
                type="number"
                min="1"
                value={form.series_count}
                onChange={(e) => setForm(f => ({ ...f, series_count: e.target.value }))}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">Parallel Count</label>
              <input
                type="number"
                min="1"
                value={form.parallel_count}
                onChange={(e) => setForm(f => ({ ...f, parallel_count: e.target.value }))}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
          </div>
          <div className="p-3 bg-gray-50 rounded-lg text-sm text-gray-600">
            Configuration: <span className="font-semibold">{form.series_count}S{form.parallel_count}P</span> ({(parseInt(form.series_count) || 0) * (parseInt(form.parallel_count) || 0)} cells)
          </div>
          <button
            type="submit"
            disabled={submitting}
            className={`w-full py-3 px-4 rounded-md text-sm font-medium text-white transition ${
              submitting ? 'bg-blue-300 cursor-not-allowed' : 'bg-blue-600 hover:bg-blue-700'
            }`}
          >
            {submitting ? 'Creating...' : 'Add Pack'}
          </button>
        </form>
      </div>
    </div>
  );
};

// Main Dashboard Component
const Dashboard = () => {
  const { user, logout } = useAuth();
  const navigate = useNavigate();

  const [currentTime, setCurrentTime] = useState(new Date());
  const [isConnected, setIsConnected] = useState(true);
  const [batteryPacks, setBatteryPacks] = useState([]);
  const [historicalData, setHistoricalData] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [refreshKey, setRefreshKey] = useState(0);
  const [showAddPack, setShowAddPack] = useState(false);
  const [deletingPackId, setDeletingPackId] = useState(null);
  const [userPacks, setUserPacks] = useState([]);

  // Add this ref to track last update time
  const lastUpdateTime = useRef(null);

  const handleLogout = () => {
    logout();
    navigate('/');
  };

  // Fetch user's registered packs
  const fetchUserPacks = useCallback(async () => {
    try {
      const data = await apiFetch('/v1/packs');
      setUserPacks(data);
    } catch (err) {
      if (err.status === 401) {
        logout();
        navigate('/login');
      }
    }
  }, [logout, navigate]);

  // Delete a pack
  const handleDeletePack = async (packId) => {
    if (!confirm('Are you sure you want to remove this battery pack?')) return;
    setDeletingPackId(packId);
    try {
      await apiFetch(`/v1/packs/${packId}`, { method: 'DELETE' });
      fetchUserPacks();
      setRefreshKey(k => k + 1);
    } catch (err) {
      console.error('Failed to delete pack:', err);
    } finally {
      setDeletingPackId(null);
    }
  };

  // Fetch battery data
  const fetchBatteryData = useCallback(async () => {
    try {
      setError(null);
      const data = await apiFetch('/v1/packs/data/latest');

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
    } catch (err) {
      if (err.status === 401) {
        logout();
        navigate('/login');
        return;
      }
      console.error("Error fetching battery data:", err);
      setError(err.message);
      setIsConnected(false);
      setLoading(false);
    }
  }, [logout, navigate]);

  // Update current time
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Fetch user packs on mount
  useEffect(() => {
    fetchUserPacks();
  }, [fetchUserPacks, refreshKey]);

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
    const accentColor = {
      safe: 'bg-emerald-500',
      caution: 'bg-amber-500',
      alert: 'bg-rose-500'
    }[pack.status] || 'bg-gray-400';

    const socColor = pack.soc > 50 ? 'bg-slate-700' : pack.soc > 20 ? 'bg-amber-500' : 'bg-rose-500';
    const sohColor = pack.soh > 80 ? 'bg-slate-700' : pack.soh > 60 ? 'bg-amber-500' : 'bg-rose-500';

    return (
      <div className="bg-white rounded-xl border border-gray-200 shadow-sm hover:shadow-md transition-shadow duration-200 overflow-hidden">
        {/* Thin accent bar */}
        <div className={`h-0.5 ${accentColor}`} />

        <div className="p-5">
          {/* Header */}
          <div className="flex items-center justify-between mb-5">
            <div className="flex items-center gap-3">
              <div className="h-10 w-10 rounded-lg bg-slate-100 flex items-center justify-center">
                <Battery className="h-5 w-5 text-slate-600" />
              </div>
              <div>
                <h3 className="text-base font-semibold text-gray-900">{pack.name}</h3>
                <p className="text-xs text-gray-400 font-mono">{pack.id}</p>
              </div>
            </div>
            <StatusBadge status={pack.status} />
          </div>

          {/* SOC / SOH */}
          <div className="grid grid-cols-2 gap-5 mb-5">
            {[
              { label: 'SOC', value: pack.soc, barColor: socColor },
              { label: 'SOH', value: pack.soh, barColor: sohColor }
            ].map(({ label, value, barColor }) => (
              <div key={label}>
                <div className="flex items-baseline justify-between mb-1.5">
                  <span className="text-xs font-medium text-gray-400 uppercase tracking-wide">{label}</span>
                  <span className="text-xl font-bold text-gray-900">{value}%</span>
                </div>
                <div className="w-full bg-gray-100 rounded-full h-1.5">
                  <div className={`${barColor} h-1.5 rounded-full transition-all duration-300`} style={{ width: `${value}%` }} />
                </div>
              </div>
            ))}
          </div>

          {/* Electrical readings */}
          <div className="grid grid-cols-3 gap-3 mb-5">
            {[
              { label: 'Voltage', value: pack.voltage, unit: 'V' },
              { label: 'Current', value: pack.current, unit: 'A' },
              { label: 'Temp', value: pack.temp, unit: '°C' }
            ].map(({ label, value, unit }) => (
              <div key={label} className="bg-gray-50 rounded-lg px-3 py-2.5 text-center">
                <div className="text-base font-bold text-gray-900">{value}<span className="text-xs font-normal text-gray-400 ml-0.5">{unit}</span></div>
                <div className="text-[10px] text-gray-400 font-medium uppercase tracking-wider mt-0.5">{label}</div>
              </div>
            ))}
          </div>

          {/* Cell grid */}
          <BatteryGrid cells={pack.cells} config={pack.config} series={pack.series} parallel={pack.parallel} />

          {/* Footer */}
          <div className="flex items-center gap-1.5 mt-4 pt-3 border-t border-gray-100 text-[11px] text-gray-400">
            <Clock className="h-3 w-3" />
            <span>{currentTime.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: true })}</span>
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
                safe: 'bg-emerald-500',
                caution: 'bg-amber-500',
                alert: 'bg-rose-500'
              };
              return (
                <div key={status} className="flex items-center gap-1.5">
                  <div className={`w-2 h-2 ${colors[status]} rounded-full`}></div>
                  <span className="text-gray-500 text-xs">
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
            <div className="col-span-2 text-center py-12">
              <Battery className="h-12 w-12 text-gray-300 mx-auto mb-4" />
              <p className="text-gray-500 mb-2">No battery packs yet</p>
              <p className="text-sm text-gray-400 mb-4">Click the + button to register your first battery pack</p>
              <button
                onClick={() => setShowAddPack(true)}
                className="inline-flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition"
              >
                <Plus className="h-4 w-4" />
                Add Pack
              </button>
            </div>
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
            {user && (
              <div className="flex items-center gap-3 pl-3 border-l border-gray-200">
                <span className="text-sm font-medium text-gray-700">
                  {user.first_name} {user.last_name}
                </span>
                <button
                  onClick={handleLogout}
                  className="flex items-center gap-1 px-3 py-1.5 text-sm text-red-600 hover:bg-red-50 rounded-lg transition-colors"
                  title="Log out"
                >
                  <LogOut className="h-4 w-4" />
                  Logout
                </button>
              </div>
            )}
          </div>
        </div>

        {/* Registered Packs Bar */}
        {userPacks.length > 0 && (
          <div className="mb-6 bg-white rounded-lg p-4 shadow-sm border border-gray-200">
            <div className="flex items-center justify-between mb-3">
              <h3 className="text-sm font-semibold text-gray-700">Your Battery Packs</h3>
              <button
                onClick={() => setShowAddPack(true)}
                className="flex items-center gap-1 px-3 py-1 text-xs font-medium text-blue-600 hover:bg-blue-50 rounded-lg transition-colors"
              >
                <Plus className="h-3 w-3" />
                Add Pack
              </button>
            </div>
            <div className="flex flex-wrap gap-2">
              {userPacks.map((pack) => (
                <div key={pack.id} className="flex items-center gap-2 px-3 py-1.5 bg-gray-50 rounded-lg border border-gray-200 text-sm">
                  <Battery className="h-3.5 w-3.5 text-blue-500" />
                  <span className="font-medium text-gray-700">{pack.name}</span>
                  <span className="text-gray-400 text-xs">({pack.series_count}S{pack.parallel_count}P)</span>
                  <button
                    onClick={() => handleDeletePack(pack.id)}
                    disabled={deletingPackId === pack.id}
                    className="ml-1 p-0.5 text-gray-400 hover:text-red-500 transition-colors"
                    title="Remove pack"
                  >
                    <Trash2 className="h-3 w-3" />
                  </button>
                </div>
              ))}
            </div>
          </div>
        )}

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

      {/* Floating Add Pack Button */}
      <button
        onClick={() => setShowAddPack(true)}
        className="fixed bottom-8 right-8 h-14 w-14 bg-blue-600 hover:bg-blue-700 text-white rounded-full shadow-lg hover:shadow-xl transition-all flex items-center justify-center z-40"
        title="Add battery pack"
      >
        <Plus className="h-6 w-6" />
      </button>

      {/* Add Pack Modal */}
      <AddPackModal
        isOpen={showAddPack}
        onClose={() => setShowAddPack(false)}
        onPackCreated={() => {
          fetchUserPacks();
          setRefreshKey(k => k + 1);
        }}
      />
    </div>
  );
};

export default Dashboard;