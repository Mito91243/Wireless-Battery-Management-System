import React, { useState } from 'react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  ResponsiveContainer,
} from 'recharts';

const Dashboard = () => {
  const [activeTab, setActiveTab] = useState('summary');
  const [showNotifications, setShowNotifications] = useState(false);

  // Sample data for the line charts
  const systemTrendsData = [
    { time: '12:00 AM', voltage: 48.5, temperature: 25, current: 41 },
    { time: '01:00 AM', voltage: 48.2, temperature: 24.8, current: 40.8 },
    { time: '02:00 AM', voltage: 47.9, temperature: 24.5, current: 40.5 },
    { time: '03:00 AM', voltage: 48.1, temperature: 24.7, current: 40.9 },
    { time: '04:00 AM', voltage: 48.3, temperature: 25.1, current: 41.2 },
    { time: '05:00 AM', voltage: 48.0, temperature: 24.9, current: 40.7 },
    { time: '06:00 AM', voltage: 48.4, temperature: 25.3, current: 41.5 },
    { time: '07:00 AM', voltage: 48.2, temperature: 25.0, current: 41.1 },
    { time: '08:00 AM', voltage: 47.8, temperature: 24.6, current: 40.3 },
    { time: '09:00 AM', voltage: 48.1, temperature: 24.8, current: 40.9 },
    { time: '10:00 AM', voltage: 48.5, temperature: 25.2, current: 41.3 },
    { time: '11:00 AM', voltage: 48.3, temperature: 25.0, current: 41.0 },
  ];

  const batteryPacks = [
    {
      id: 'BP001',
      name: 'Pack Alpha',
      soc: 85,
      soh: 98,
      voltage: 48.10,
      current: 12.83,
      temp: 27.14,
      status: 'Safe',
      statusColor: 'green',
      cellVoltages: [3.79, 3.68, 3.62, 3.58, 3.69, 3.59, 3.76, 3.68, 3.60, 3.58, 3.70, 3.60]
    },
    {
      id: 'BP002',
      name: 'Pack Beta',
      soc: 62,
      soh: 95,
      voltage: 46.80,
      current: 8.23,
      temp: 32.01,
      status: 'Caution',
      statusColor: 'yellow',
      cellVoltages: [3.67, 3.58, 3.69, 3.61, 3.68, 3.60, 3.65, 3.56, 3.58, 3.58, 3.62, 3.63]
    },
    {
      id: 'BP003',
      name: 'Pack Gamma',
      soc: 91,
      soh: 97,
      voltage: 49.20,
      current: 15.63,
      temp: 25.89,
      status: 'Safe',
      statusColor: 'green',
      cellVoltages: [3.83, 3.75, 3.77, 3.67, 3.61, 3.62, 3.80, 3.76, 3.66, 3.72, 3.73, 3.71]
    },
    {
      id: 'BP004',
      name: 'Pack Delta',
      soc: 23,
      soh: 89,
      voltage: 42.10,
      current: 5.04,
      temp: 34.91,
      status: 'Alert',
      statusColor: 'red',
      cellVoltages: [3.19, 3.16, 3.13, 3.21, 3.31, 3.18, 3.16, 3.24, 3.22, 3.17, 3.35, 3.20]
    }
  ];

  const historyData = [
    { id: 'BP001', name: 'Pack Alpha', soc: '85%', soh: '98%', voltage: '48.10V', current: '11.67A', temp: '28.71°C', status: 'Safe', lastUpdate: '13:50:00 PM' },
    { id: 'BP002', name: 'Pack Beta', soc: '62%', soh: '95%', voltage: '46.80V', current: '8.95A', temp: '30.58°C', status: 'Caution', lastUpdate: '13:50:00 PM' },
    { id: 'BP003', name: 'Pack Gamma', soc: '91%', soh: '97%', voltage: '49.20V', current: '15.76A', temp: '25.16°C', status: 'Safe', lastUpdate: '13:50:00 PM' },
    { id: 'BP004', name: 'Pack Delta', soc: '23%', soh: '89%', voltage: '42.10V', current: '6.02A', temp: '34.68°C', status: 'Alert', lastUpdate: '13:50:00 PM' }
  ];

  const StatusCard = ({ title, value, unit, color, icon }) => (
    <div className="bg-white p-6 rounded-lg shadow-sm">
      <div className="flex justify-between items-center mb-3">
        <span className="text-sm text-gray-500">{title}</span>
        <div className={`p-2 rounded-lg ${
          color === 'blue' ? 'bg-blue-100 text-blue-600' :
          color === 'green' ? 'bg-green-100 text-green-600' :
          color === 'red' ? 'bg-red-100 text-red-600' :
          'bg-gray-100 text-gray-600'
        }`}>
          {icon}
        </div>
      </div>
      <div className="text-2xl font-semibold text-gray-900">
        {value} <span className="text-base font-normal text-gray-500">{unit}</span>
      </div>
    </div>
  );

  const BatteryPackCard = ({ pack }) => (
    <div className={`bg-white rounded-lg shadow-sm p-5 border-l-4 ${
      pack.statusColor === 'green' ? 'border-green-500 bg-green-50' :
      pack.statusColor === 'yellow' ? 'border-yellow-500 bg-yellow-50' :
      pack.statusColor === 'red' ? 'border-red-500 bg-red-50' :
      'border-gray-300'
    }`}>
      <div className="flex justify-between items-center mb-4">
        <div className="flex items-center space-x-2">
          <span className="text-xs text-gray-500">{pack.id}</span>
          <span className="text-base font-semibold text-gray-900">{pack.name}</span>
        </div>
        <div className={`px-3 py-1 rounded-full text-xs font-medium ${
          pack.statusColor === 'green' ? 'bg-green-100 text-green-800' :
          pack.statusColor === 'yellow' ? 'bg-yellow-100 text-yellow-800' :
          pack.statusColor === 'red' ? 'bg-red-100 text-red-800' :
          'bg-gray-100 text-gray-800'
        }`}>
          {pack.status}
        </div>
      </div>
      
      <div className="space-y-4">
        <div className="flex space-x-6">
          <div className="flex-1">
            <div className="text-xs text-gray-500 mb-1">State of Charge</div>
            <div className="text-lg font-semibold text-gray-900 mb-2">{pack.soc}%</div>
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div 
                className="bg-blue-600 h-2 rounded-full transition-all duration-300" 
                style={{ width: `${pack.soc}%` }}
              ></div>
            </div>
          </div>
          <div className="flex-1">
            <div className="text-xs text-gray-500 mb-1">State of Health</div>
            <div className="text-lg font-semibold text-gray-900 mb-2">{pack.soh}%</div>
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div 
                className="bg-blue-600 h-2 rounded-full transition-all duration-300" 
                style={{ width: `${pack.soh}%` }}
              ></div>
            </div>
          </div>
        </div>
        
        <div className="flex justify-between text-center">
          <div className="flex-1">
            <div className="text-xs text-gray-500">Voltage (V)</div>
            <div className="text-sm font-semibold text-gray-900">{pack.voltage}</div>
          </div>
          <div className="flex-1">
            <div className="text-xs text-gray-500">Current (A)</div>
            <div className="text-sm font-semibold text-gray-900">{pack.current}</div>
          </div>
          <div className="flex-1">
            <div className="text-xs text-gray-500">Temp (°C)</div>
            <div className="text-sm font-semibold text-gray-900">{pack.temp}</div>
          </div>
        </div>
        
        <div>
          <div className="text-xs text-gray-500 mb-2">Cell Voltages</div>
          <div className="grid grid-cols-6 gap-1 mb-3">
            {pack.cellVoltages.map((voltage, index) => (
              <div key={index} className="bg-gray-100 text-center py-1 px-1 rounded text-xs text-gray-700">
                {voltage}
              </div>
            ))}
          </div>
          <div className="flex justify-end">
            <button className="text-xs px-3 py-1 border border-gray-300 rounded text-gray-600 hover:bg-gray-50">
              History
            </button>
          </div>
        </div>
      </div>
    </div>
  );

  const renderSummaryTab = () => (
    <div className="space-y-8">
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <StatusCard 
          title="Total Power" 
          value="7.77" 
          unit="kW" 
          color="blue"
          icon="⚡"
        />
        <StatusCard 
          title="Total Current" 
          value="41.70" 
          unit="A" 
          color="green"
          icon="🔋"
        />
        <StatusCard 
          title="Active Packs" 
          value="4" 
          unit="" 
          color="blue"
          icon="📊"
        />
        <StatusCard 
          title="Active Alerts" 
          value="2" 
          unit="1 critical 1 warning" 
          color="red"
          icon="⚠️"
        />
      </div>

      <div className="bg-white rounded-lg shadow-sm p-6">
        <h3 className="text-xl font-semibold text-gray-900 mb-2">System Performance Overview</h3>
        <p className="text-gray-600 mb-6">Real-time voltage and temperature monitoring across all battery packs</p>
        
        <div>
          <h4 className="text-lg font-medium text-gray-900 mb-4">Live System Trends</h4>
          <div className="h-80">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={systemTrendsData}>
                <CartesianGrid strokeDasharray="3 3" className="opacity-30" />
                <XAxis 
                  dataKey="time" 
                  tick={{ fontSize: 12 }}
                  tickLine={false}
                />
                <YAxis tick={{ fontSize: 12 }} tickLine={false} />
                <Line 
                  type="monotone" 
                  dataKey="voltage" 
                  stroke="#3b82f6" 
                  strokeWidth={2}
                  dot={false}
                  name="Voltage (V)"
                />
                <Line 
                  type="monotone" 
                  dataKey="temperature" 
                  stroke="#f59e0b" 
                  strokeWidth={2}
                  dot={false}
                  name="Temperature (°C)"
                />
                <Line 
                  type="monotone" 
                  dataKey="current" 
                  stroke="#10b981" 
                  strokeWidth={2}
                  dot={false}
                  name="Current (A)"
                />
              </LineChart>
            </ResponsiveContainer>
          </div>
          <div className="flex justify-center space-x-6 mt-4 text-sm">
            <div className="flex items-center">
              <div className="w-4 h-0.5 bg-blue-500 mr-2"></div>
              <span className="text-gray-600">Voltage (V)</span>
            </div>
            <div className="flex items-center">
              <div className="w-4 h-0.5 bg-yellow-500 mr-2"></div>
              <span className="text-gray-600">Temperature (°C)</span>
            </div>
            <div className="flex items-center">
              <div className="w-4 h-0.5 bg-green-500 mr-2"></div>
              <span className="text-gray-600">Current (A)</span>
            </div>
          </div>
        </div>
      </div>

      <div>
        <h3 className="text-xl font-semibold text-gray-900 mb-6">Battery Pack Status</h3>
        <div className="grid grid-cols-1 lg:grid-cols-2 xl:grid-cols-4 gap-6">
          {batteryPacks.map((pack, index) => (
            <BatteryPackCard key={index} pack={pack} />
          ))}
        </div>
      </div>
    </div>
  );

  const renderChartsTab = () => (
    <div className="bg-white rounded-lg shadow-sm p-6">
      <div className="space-y-8">
        <div>
          <h3 className="text-lg font-semibold text-gray-900 mb-4">Voltage & Temperature Trends (24h)</h3>
          <div className="h-64">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={systemTrendsData}>
                <CartesianGrid strokeDasharray="3 3" className="opacity-30" />
                <XAxis dataKey="time" tick={{ fontSize: 12 }} />
                <YAxis tick={{ fontSize: 12 }} />
                <Line type="monotone" dataKey="voltage" stroke="#3b82f6" strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="temperature" stroke="#f59e0b" strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="current" stroke="#10b981" strokeWidth={2} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>
        
        <div>
          <h3 className="text-lg font-semibold text-gray-900 mb-4">Current & Power Trends (24h)</h3>
          <div className="h-64">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={systemTrendsData}>
                <CartesianGrid strokeDasharray="3 3" className="opacity-30" />
                <XAxis dataKey="time" tick={{ fontSize: 12 }} />
                <YAxis tick={{ fontSize: 12 }} />
                <Line type="monotone" dataKey="voltage" stroke="#3b82f6" strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="temperature" stroke="#f59e0b" strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="current" stroke="#10b981" strokeWidth={2} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>
        
        <div>
          <h3 className="text-lg font-semibold text-gray-900 mb-4">System Efficiency Overview (24h)</h3>
          <div className="h-64">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={systemTrendsData}>
                <CartesianGrid strokeDasharray="3 3" className="opacity-30" />
                <XAxis dataKey="time" tick={{ fontSize: 12 }} />
                <YAxis tick={{ fontSize: 12 }} />
                <Line type="monotone" dataKey="voltage" stroke="#3b82f6" strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="temperature" stroke="#f59e0b" strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="current" stroke="#10b981" strokeWidth={2} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>
        
        <div>
          <h3 className="text-lg font-semibold text-gray-900 mb-4">Battery Health Monitoring (24h)</h3>
          <div className="h-64">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={systemTrendsData}>
                <CartesianGrid strokeDasharray="3 3" className="opacity-30" />
                <XAxis dataKey="time" tick={{ fontSize: 12 }} />
                <YAxis tick={{ fontSize: 12 }} />
                <Line type="monotone" dataKey="voltage" stroke="#3b82f6" strokeWidth={2} dot={false} />
                <Line type="monotone" dataKey="temperature" stroke="#f59e0b" strokeWidth={2} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>
      </div>
    </div>
  );

  const renderHistoryTab = () => (
    <div className="bg-white rounded-lg shadow-sm p-6">
      <h3 className="text-xl font-semibold text-gray-900 mb-2">Battery Pack History</h3>
      <p className="text-gray-600 mb-6">Click on any row to view detailed history</p>
      
      <div className="overflow-x-auto mb-6">
        <table className="min-w-full divide-y divide-gray-200">
          <thead className="bg-gray-50">
            <tr>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Pack ID</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Name</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">SOC (%)</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">SOH (%)</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Voltage (V)</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Current (A)</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Temperature (°C)</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Status</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Last Update</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">Action</th>
            </tr>
          </thead>
          <tbody className="bg-white divide-y divide-gray-200">
            {historyData.map((row, index) => (
              <tr key={index} className="hover:bg-gray-50">
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{row.id}</td>
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{row.name}</td>
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{row.soc}</td>
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{row.soh}</td>
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{row.voltage}</td>
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{row.current}</td>
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{row.temp}</td>
                <td className="px-6 py-4 whitespace-nowrap">
                  <span className={`inline-flex px-2 py-1 text-xs font-semibold rounded-full ${
                    row.status === 'Safe' ? 'bg-green-100 text-green-800' :
                    row.status === 'Caution' ? 'bg-yellow-100 text-yellow-800' :
                    row.status === 'Alert' ? 'bg-red-100 text-red-800' :
                    'bg-gray-100 text-gray-800'
                  }`}>
                    {row.status}
                  </span>
                </td>
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-900">{row.lastUpdate}</td>
                <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                  <button className="text-gray-400 hover:text-gray-600">⋯</button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      
      <div className="flex justify-center">
        <button className="bg-gray-900 text-white px-6 py-2 rounded-md text-sm hover:bg-gray-800 transition-colors">
          Starting live preview...
        </button>
      </div>
    </div>
  );

  return (
    <div className="min-h-screen bg-gray-50">
      {/* Header */}
      <div className="bg-white shadow-sm border-b border-gray-200">
        <div className="px-6 py-4">
          <div className="flex justify-between items-center">
            <div>
              <h1 className="text-2xl font-semibold text-gray-900 flex items-center">
                🔋 Battery Management System
              </h1>
              <p className="text-sm text-gray-600 mt-1">Real-time monitoring and control dashboard</p>
            </div>
            <div className="flex items-center space-x-4">
              <button className="bg-blue-600 text-white px-4 py-2 rounded-md text-sm font-medium hover:bg-blue-700 transition-colors">
                Connected
              </button>
              <div className="relative">
                <button 
                  className="relative p-2 text-gray-600 hover:text-gray-900 transition-colors"
                  onClick={() => setShowNotifications(!showNotifications)}
                >
                  🔔
                  <span className="absolute -top-1 -right-1 bg-red-500 text-white text-xs rounded-full h-5 w-5 flex items-center justify-center">
                    1
                  </span>
                </button>
                {showNotifications && (
                  <div className="absolute right-0 mt-2 w-80 bg-white rounded-md shadow-lg border border-gray-200 z-50">
                    <div className="p-4">
                      <div className="flex items-center justify-between mb-3">
                        <h3 className="text-sm font-medium text-gray-900">Notifications</h3>
                        <span className="text-xs text-gray-500">2 active notifications</span>
                      </div>
                      <div className="space-y-3">
                        <div className="flex items-start space-x-3">
                          <div className="flex-shrink-0">
                            <span className="inline-flex items-center justify-center h-8 w-8 rounded-full bg-red-100">
                              <span className="text-red-600 text-sm">⚠️</span>
                            </span>
                          </div>
                          <div className="flex-1 min-w-0">
                            <p className="text-sm font-medium text-gray-900">Critical Alerts</p>
                            <p className="text-sm text-gray-600">1 battery pack(s) in critical state</p>
                            <p className="text-xs text-gray-500">5 min ago</p>
                          </div>
                        </div>
                        <div className="flex items-start space-x-3">
                          <div className="flex-shrink-0">
                            <span className="inline-flex items-center justify-center h-8 w-8 rounded-full bg-yellow-100">
                              <span className="text-yellow-600 text-sm">⚡</span>
                            </span>
                          </div>
                          <div className="flex-1 min-w-0">
                            <p className="text-sm font-medium text-gray-900">Caution Warnings</p>
                            <p className="text-sm text-gray-600">1 battery pack(s) show warning</p>
                            <p className="text-xs text-gray-500">12 min ago</p>
                          </div>
                        </div>
                      </div>
                      <div className="mt-4 pt-3 border-t border-gray-100">
                        <button className="w-full text-center text-sm text-blue-600 hover:text-blue-800 font-medium">
                          View all notifications
                        </button>
                      </div>
                    </div>
                  </div>
                )}
              </div>
              <div className="flex items-center space-x-3 text-sm text-gray-600">
                <span>josephsagy@gmail.com</span>
                <button className="text-gray-500 hover:text-gray-700 transition-colors">Sign Out</button>
              </div>
            </div>
          </div>
        </div>

        {/* Navigation Tabs */}
        <div className="px-6">
          <nav className="flex space-x-8">
            <button 
              className={`py-4 px-1 border-b-2 font-medium text-sm ${
                activeTab === 'summary' 
                  ? 'border-blue-500 text-blue-600' 
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
              onClick={() => setActiveTab('summary')}
            >
              📊 Summary & Stats
            </button>
            <button 
              className={`py-4 px-1 border-b-2 font-medium text-sm ${
                activeTab === 'charts' 
                  ? 'border-blue-500 text-blue-600' 
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
              onClick={() => setActiveTab('charts')}
            >
              📈 Charts
            </button>
            <button 
              className={`py-4 px-1 border-b-2 font-medium text-sm ${
                activeTab === 'history' 
                  ? 'border-blue-500 text-blue-600' 
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
              onClick={() => setActiveTab('history')}
            >
              🕒 History
            </button>
          </nav>
        </div>
      </div>

      {/* Main Content */}
      <div className="px-6 py-8">
        {activeTab === 'summary' && renderSummaryTab()}
        {activeTab === 'charts' && renderChartsTab()}
        {activeTab === 'history' && renderHistoryTab()}
      </div>
    </div>
  );
};

export default Dashboard;