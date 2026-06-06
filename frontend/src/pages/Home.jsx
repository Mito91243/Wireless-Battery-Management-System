import { Upload, Battery, Radio, Zap, CheckCircle } from "lucide-react";
import { Link } from "react-router-dom";
import { useAuth } from "../context/AuthContext";


export default function HomePage() {
  const { isAuthenticated } = useAuth();

  const features = [
    {
      icon: <Radio className="w-8 h-8 text-blue-500" />, 
      title: "Wireless Monitoring",
      description:
        "Monitor battery performance in real-time without the need for wired connections.",
    },
    {
      icon: <Battery className="w-8 h-8 text-green-500" />, 
      title: "Smart Battery Health",
      description:
        "Track voltage, temperature, and state-of-charge with intelligent algorithms.",
    },
    {
      icon: <Zap className="w-8 h-8 text-yellow-500" />, 
      title: "Efficient Energy Management",
      description:
        "Balance cells automatically to improve efficiency and extend battery life.",
    },
    {
      icon: <Upload className="w-8 h-8 text-purple-500" />, 
      title: "Data Logging & Insights",
      description:
        "Store and analyze performance data to optimize future battery usage.",
    },
  ];

  const benefits = [
    {
      title: "Enhanced Safety",
      description:
        "Prevent overheating, overcharging, and potential failures with real-time alerts.",
    },
    {
      title: "Longer Battery Life",
      description:
        "Optimized balancing ensures batteries last longer and perform better.",
    },
    {
      title: "Cutting-Edge Research",
      description:
        "An innovative approach to battery management using wireless technology.",
    },
  ];

  return (
    <div className="min-h-screen bg-white overflow-x-hidden">
      {/* Hero Section */}
      <section className="relative overflow-hidden bg-gradient-to-b from-blue-50/70 via-white to-white">
        {/* Ambient background glows */}
        <div className="pointer-events-none absolute inset-0">
          <div className="absolute -top-24 -left-24 h-72 w-72 rounded-full bg-blue-300/30 blur-3xl" />
          <div className="absolute top-1/3 right-0 h-80 w-80 rounded-full bg-cyan-300/20 blur-3xl" />
          <div
            className="absolute inset-0 opacity-[0.04]"
            style={{
              backgroundImage:
                "linear-gradient(#0f172a 1px, transparent 1px), linear-gradient(90deg, #0f172a 1px, transparent 1px)",
              backgroundSize: "44px 44px",
            }}
          />
        </div>

        <div className="relative max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-16 sm:py-20 lg:py-28">
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-12 lg:gap-10 items-center">
            {/* Left - Copy */}
            <div className="text-center lg:text-left">
              <h1 className="text-4xl sm:text-5xl lg:text-6xl font-bold tracking-tight leading-[1.1] text-gray-900 break-words">
                Wireless{" "}
                <span className="bg-gradient-to-r from-blue-600 via-cyan-500 to-blue-500 bg-clip-text text-transparent">
                  Battery Management
                </span>{" "}
                System
              </h1>

              <p className="mt-6 text-base sm:text-lg text-gray-600 max-w-xl mx-auto lg:mx-0">
                A next-generation BMS where wireless slave nodes stream live cell
                data to a master controller — delivering safer, smarter, and more
                efficient battery monitoring in real time.
              </p>

              <div className="mt-9 flex flex-col sm:flex-row gap-4 justify-center lg:justify-start items-center">
                <Link to={isAuthenticated ? "/dashboard" : "/login"} className="w-full sm:w-auto">
                  <button className="w-full px-8 py-3.5 bg-gradient-to-r from-blue-600 to-cyan-500 text-white font-semibold rounded-xl shadow-lg shadow-blue-500/25 hover:from-blue-500 hover:to-cyan-400 focus:outline-none focus:ring-2 focus:ring-cyan-500 focus:ring-offset-2 transition duration-200">
                    View Prototype
                  </button>
                </Link>
                <Link to="/documentation" className="w-full sm:w-auto">
                  <button className="w-full px-8 py-3.5 bg-white text-gray-700 font-semibold rounded-xl border border-gray-300 hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-gray-400 focus:ring-offset-2 transition duration-200">
                    Learn More
                  </button>
                </Link>
              </div>

              <div className="mt-10 flex flex-wrap justify-center lg:justify-start gap-x-8 gap-y-3 text-sm text-gray-500">
                <div className="flex items-center gap-2">
                  <Radio className="w-4 h-4 text-cyan-500" /> Wireless telemetry
                </div>
                <div className="flex items-center gap-2">
                  <Zap className="w-4 h-4 text-yellow-500" /> Real-time balancing
                </div>
                <div className="flex items-center gap-2">
                  <Battery className="w-4 h-4 text-green-500" /> Live cell health
                </div>
              </div>
            </div>

            {/* Right - Animated network diagram */}
            <div className="relative">
              <NetworkAnimation />
            </div>
          </div>
        </div>
      </section>

      {/* Features Section */}
      <section className="py-16 sm:py-20 px-4 sm:px-6 lg:px-8 bg-white">
        <div className="max-w-7xl mx-auto">
          <div className="text-center mb-12 sm:mb-16">
            <h2 className="text-2xl sm:text-3xl md:text-4xl font-bold text-gray-900 mb-4 break-words">
              Key Features of the Wireless BMS
            </h2>
            <p className="text-base sm:text-lg md:text-xl text-gray-600 max-w-3xl mx-auto px-2">
              Innovative technology designed to improve battery safety, performance,
              and monitoring.
            </p>
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-8">
            {features.map((feature, index) => (
              <div key={index} className="text-center">
                <div className="bg-gray-50 rounded-2xl p-4 w-16 h-16 mx-auto mb-6 flex items-center justify-center">
                  {feature.icon}
                </div>
                <h3 className="text-lg sm:text-xl font-semibold text-gray-900 mb-4 break-words">
                  {feature.title}
                </h3>
                <p className="text-gray-600 leading-relaxed break-words">
                  {feature.description}
                </p>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* Why Choose Section */}
      <section className="py-16 sm:py-20 px-4 sm:px-6 lg:px-8 bg-gray-50">
        <div className="max-w-7xl mx-auto">
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-10 lg:gap-16 items-center">
            {/* Left Side - Benefits */}
            <div>
              <h2 className="text-2xl sm:text-3xl md:text-4xl font-bold text-gray-900 mb-8 sm:mb-12 break-words">
                Why This Project Matters
              </h2>

              <div className="space-y-6 sm:space-y-8">
                {benefits.map((benefit, index) => (
                  <div key={index} className="flex items-start">
                    <div className="flex-shrink-0 mr-4">
                      <CheckCircle className="w-6 h-6 text-green-500" />
                    </div>
                    <div className="min-w-0">
                      <h3 className="text-lg sm:text-xl font-semibold text-gray-900 mb-2 break-words">
                        {benefit.title}
                      </h3>
                      <p className="text-gray-600 leading-relaxed break-words">
                        {benefit.description}
                      </p>
                    </div>
                  </div>
                ))}
              </div>
            </div>

            {/* Right Side - CTA */}
            <div className="bg-white rounded-2xl p-6 sm:p-8 shadow-lg border border-gray-200">
              <h3 className="text-xl sm:text-2xl font-bold text-gray-900 mb-4 text-center break-words">
                Explore the Project
              </h3>
              <p className="text-gray-600 text-center mb-6 sm:mb-8">
                A step forward in safe and efficient energy storage systems.
              </p>

              <div className="text-center">
                <Link to="/documentation" className="block">
                  <button className="w-full px-8 py-4 bg-blue-600 text-white font-semibold rounded-lg hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-blue-500 focus:ring-offset-2 transition duration-200 mb-4">
                    View Documentation
                  </button>
                </Link>

                <p className="text-sm text-gray-500">
                  Graduation Project • Wireless Battery Management System
                </p>
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* Footer */}
      <footer className="bg-gray-900 text-white py-10 sm:py-12 px-4 sm:px-6 lg:px-8">
        <div className="max-w-7xl mx-auto text-center">
          <h3 className="text-lg sm:text-xl font-bold text-blue-400 mb-4 break-words">
            Wireless BMS Project
          </h3>
          <p className="text-sm sm:text-base text-gray-400 mb-6 max-w-2xl mx-auto break-words">
            A graduation project focused on advancing battery safety and performance
            through wireless communication and smart management.
          </p>
          <div className="flex flex-wrap justify-center gap-x-6 gap-y-3 sm:gap-x-8 text-sm text-gray-400">
            <a href="#" className="hover:text-white transition duration-200">
              Project Report
            </a>
            <a href="#" className="hover:text-white transition duration-200">
              Prototype Demo
            </a>
            <a href="#" className="hover:text-white transition duration-200">
              Contact Me
            </a>
          </div>
        </div>
      </footer>
    </div>
  );
}

/* ----------------------------------------------------------------------------
   Animated architecture diagram — the full two-way pipeline:
     Battery (BQ76952) -> Slave ESP32 -> Master ESP32 -> Cloud (MQTT + DB) -> Dashboard
   Cyan packets carry live telemetry UP the chain; amber packets carry control
   commands BACK DOWN — so the graphic shows the system is bidirectional.
   Pure inline SVG + SMIL + CSS keyframes: no build/animation config, scales on mobile.
---------------------------------------------------------------------------- */
function NetworkAnimation() {
  const NODE_W = 104;
  const NODE_H = 104;
  const CY = 118; // vertical center of the node row
  const TOP = CY - NODE_H / 2; // 66
  // Five stages, evenly spaced left -> right.
  const stages = [
    { id: "pack", cx: 80, label: "BATTERY", sub: "BQ76952" },
    { id: "slave", cx: 232, label: "SLAVE", sub: "ESP32" },
    { id: "master", cx: 384, label: "MASTER", sub: "ESP32 hub" },
    { id: "cloud", cx: 536, label: "CLOUD", sub: "MQTT · DB" },
    { id: "dash", cx: 688, label: "DASHBOARD", sub: "Users" },
  ];
  const protocols = ["I2C", "ESP-NOW", "MQTT", "HTTPS"];

  return (
    <div className="mx-auto w-full max-w-[560px] animate-[float_6s_ease-in-out_infinite]">
      <style>{`
        @keyframes float { 0%, 100% { transform: translateY(0); } 50% { transform: translateY(-10px); } }
        @keyframes dashflow { to { stroke-dashoffset: -16; } }
      `}</style>
      <svg
        viewBox="0 0 768 250"
        className="w-full h-auto"
        role="img"
        aria-label="Animated diagram of the wireless battery management system: a battery pack, a slave ESP32, a master ESP32, a cloud with an MQTT broker and database, and a user dashboard. Cyan packets show telemetry flowing up to the cloud; amber packets show control commands flowing back down to the pack."
      >
        <defs>
          <linearGradient id="nodeFill" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" stopColor="#ffffff" />
            <stop offset="100%" stopColor="#eef2f7" />
          </linearGradient>
          <radialGradient id="hubGlow" cx="50%" cy="50%" r="50%">
            <stop offset="0%" stopColor="#06b6d4" stopOpacity="0.25" />
            <stop offset="100%" stopColor="#06b6d4" stopOpacity="0" />
          </radialGradient>
          <filter id="soft" x="-40%" y="-40%" width="180%" height="180%">
            <feDropShadow dx="0" dy="4" stdDeviation="5" floodColor="#1e3a8a" floodOpacity="0.12" />
          </filter>
          <filter id="glow" x="-60%" y="-60%" width="220%" height="220%">
            <feGaussianBlur stdDeviation="2.4" result="b" />
            <feMerge>
              <feMergeNode in="b" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
        </defs>

        {/* Links: protocol tag + two-way animated packets for each hop */}
        {stages.slice(0, -1).map((s, i) => {
          const ax = s.cx + NODE_W / 2;
          const bx = stages[i + 1].cx - NODE_W / 2;
          const midX = (ax + bx) / 2;
          const dataY = CY - 7;
          const cmdY = CY + 7;
          const dataPath = `M ${ax} ${dataY} L ${bx} ${dataY}`;
          const cmdPath = `M ${bx} ${cmdY} L ${ax} ${cmdY}`;
          return (
            <g key={`link-${i}`}>
              <line x1={ax} y1={dataY} x2={bx} y2={dataY} stroke="#cbd5e1" strokeWidth="1.5" strokeDasharray="2 4" style={{ animation: "dashflow 1s linear infinite" }} />
              <line x1={ax} y1={cmdY} x2={bx} y2={cmdY} stroke="#fed7aa" strokeWidth="1.5" strokeDasharray="2 4" style={{ animation: "dashflow 1.4s linear infinite reverse" }} />
              <path d={`M ${bx - 5} ${dataY - 3} L ${bx} ${dataY} L ${bx - 5} ${dataY + 3}`} fill="none" stroke="#0ea5e9" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
              <path d={`M ${ax + 5} ${cmdY - 3} L ${ax} ${cmdY} L ${ax + 5} ${cmdY + 3}`} fill="none" stroke="#f59e0b" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
              <g transform={`translate(${midX} ${CY - 30})`}>
                <rect x="-23" y="-9" width="46" height="18" rx="9" fill="#ffffff" stroke="#e2e8f0" />
                <text x="0" y="3.5" textAnchor="middle" fontSize="9.5" fontWeight="700" fill="#475569" fontFamily="ui-sans-serif, system-ui">{protocols[i]}</text>
              </g>
              {[0, 0.9].map((delay, j) => (
                <circle key={`d-${j}`} r="3.4" fill="#0ea5e9" filter="url(#glow)">
                  <animateMotion dur="1.8s" begin={`${i * 0.22 + delay}s`} repeatCount="indefinite" path={dataPath} />
                </circle>
              ))}
              {[0.45, 1.35].map((delay, j) => (
                <circle key={`c-${j}`} r="3.4" fill="#f59e0b" filter="url(#glow)">
                  <animateMotion dur="1.8s" begin={`${i * 0.22 + delay}s`} repeatCount="indefinite" path={cmdPath} />
                </circle>
              ))}
            </g>
          );
        })}

        {/* Nodes */}
        {stages.map((s) => {
          const x = s.cx - NODE_W / 2;
          const isMaster = s.id === "master";
          return (
            <g key={s.id}>
              {isMaster && (
                <circle cx={s.cx} cy={CY} r="60" fill="url(#hubGlow)">
                  <animate attributeName="r" values="50;64;50" dur="3s" repeatCount="indefinite" />
                </circle>
              )}
              <rect x={x} y={TOP} width={NODE_W} height={NODE_H} rx="15" fill="url(#nodeFill)" stroke={isMaster ? "#0891b2" : "#d3dbe6"} strokeWidth={isMaster ? 2 : 1.4} filter="url(#soft)" />
              <circle cx={x + NODE_W - 13} cy={TOP + 13} r="3" fill="#22c55e">
                <animate attributeName="opacity" values="1;0.25;1" dur="1.6s" begin={`${s.cx * 0.002}s`} repeatCount="indefinite" />
              </circle>
              <Glyph id={s.id} cx={s.cx} cy={CY - 8} />
              <text x={s.cx} y={CY + 25} textAnchor="middle" fontSize="12" fontWeight="700" fill="#0f172a" fontFamily="ui-sans-serif, system-ui">{s.label}</text>
              <text x={s.cx} y={CY + 38} textAnchor="middle" fontSize="8.5" fontWeight="500" fill="#94a3b8" fontFamily="ui-sans-serif, system-ui">{s.sub}</text>
            </g>
          );
        })}

        {/* Legend — makes the two-way flow explicit */}
        <g fontFamily="ui-sans-serif, system-ui">
          <g transform="translate(238 222)">
            <circle cx="0" cy="0" r="4" fill="#0ea5e9" />
            <text x="11" y="3.5" fontSize="10.5" fontWeight="600" fill="#475569">Telemetry →</text>
          </g>
          <g transform="translate(430 222)">
            <circle cx="0" cy="0" r="4" fill="#f59e0b" />
            <text x="11" y="3.5" fontSize="10.5" fontWeight="600" fill="#475569">← Commands</text>
          </g>
        </g>
      </svg>
    </div>
  );
}

/* Per-stage monoline icon, centered at (cx, cy). */
function Glyph({ id, cx, cy }) {
  if (id === "pack") {
    return (
      <g>
        <rect x={cx - 18} y={cy - 10} width="32" height="20" rx="3" fill="none" stroke="#0ea5e9" strokeWidth="2" />
        <rect x={cx + 14} y={cy - 5} width="4" height="10" rx="1" fill="#0ea5e9" />
        <rect x={cx - 15} y={cy - 7} width="14" height="14" rx="1" fill="#0ea5e9">
          <animate attributeName="width" values="6;24;6" dur="3s" repeatCount="indefinite" />
        </rect>
      </g>
    );
  }
  if (id === "slave") {
    return (
      <g fill="none" stroke="#0ea5e9" strokeWidth="2" strokeLinecap="round">
        <rect x={cx - 13} y={cy - 13} width="26" height="26" rx="4" fill="#ffffff" />
        <circle cx={cx} cy={cy} r="4" fill="#0ea5e9" stroke="none" />
        {[-7, 0, 7].map((o) => (
          <g key={o}>
            <line x1={cx + o} y1={cy - 13} x2={cx + o} y2={cy - 17} />
            <line x1={cx + o} y1={cy + 13} x2={cx + o} y2={cy + 17} />
            <line x1={cx - 13} y1={cy + o} x2={cx - 17} y2={cy + o} />
            <line x1={cx + 13} y1={cy + o} x2={cx + 17} y2={cy + o} />
          </g>
        ))}
      </g>
    );
  }
  if (id === "master") {
    return (
      <g fill="none" stroke="#0891b2" strokeWidth="2.4" strokeLinecap="round">
        <path d={`M ${cx - 9} ${cy + 3} a 9 9 0 0 1 18 0`} />
        <path d={`M ${cx - 15} ${cy - 2} a 15 15 0 0 1 30 0`} />
        <path d={`M ${cx - 21} ${cy - 7} a 21 21 0 0 1 42 0`} opacity="0.45" />
        <circle cx={cx} cy={cy + 7} r="3" fill="#0891b2" stroke="none" />
      </g>
    );
  }
  if (id === "cloud") {
    return (
      <g transform={`translate(${cx} ${cy})`}>
        <path d="M -9 5 a 6 6 0 0 1 1 -11 a 8 8 0 0 1 15 -1 a 6 6 0 0 1 1 12 z" fill="#ffffff" stroke="#7c3aed" strokeWidth="2" strokeLinejoin="round" />
        <g fill="#ffffff" stroke="#7c3aed" strokeWidth="1.7" transform="translate(0 11)">
          <ellipse cx="0" cy="0" rx="8" ry="2.6" />
          <path d="M -8 0 v 6 a 8 2.6 0 0 0 16 0 v -6" />
          <ellipse cx="0" cy="0" rx="8" ry="2.6" fill="#ffffff" />
        </g>
      </g>
    );
  }
  // dashboard
  return (
    <g fill="none" stroke="#10b981" strokeWidth="2" strokeLinecap="round">
      <rect x={cx - 16} y={cy - 12} width="32" height="22" rx="2" fill="#ffffff" />
      <line x1={cx} y1={cy + 10} x2={cx} y2={cy + 14} />
      <line x1={cx - 7} y1={cy + 14} x2={cx + 7} y2={cy + 14} />
      {[[-9, 5], [-2, 9], [5, 6]].map(([ox, h], k) => (
        <rect key={k} x={cx + ox} y={cy + 4 - h} width="4" height={h} rx="1" fill="#10b981" stroke="none">
          <animate attributeName="height" values={`${h};${h - 3};${h}`} dur="2s" begin={`${k * 0.3}s`} repeatCount="indefinite" />
          <animate attributeName="y" values={`${cy + 4 - h};${cy + 4 - (h - 3)};${cy + 4 - h}`} dur="2s" begin={`${k * 0.3}s`} repeatCount="indefinite" />
        </rect>
      ))}
    </g>
  );
}
