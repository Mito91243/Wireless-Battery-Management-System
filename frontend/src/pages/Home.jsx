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
   Animated architecture diagram — fan-in topology:
     4 wireless slave nodes -> 1 master hub -> dashboard.
   Cyan packets stream live data from each slave into the master, then on to the
   dashboard. Pure inline SVG + SMIL + CSS keyframes: no build/animation config.
---------------------------------------------------------------------------- */
function NetworkAnimation() {
  const SLAVE_W = 92;
  const SLAVE_H = 64;
  const MASTER_W = 108;
  const MASTER_H = 108;
  const DASH_W = 100;
  const DASH_H = 88;

  // Four slaves stacked on the left, master centered, dashboard on the right.
  const slaves = [
    { cy: 56 },
    { cy: 146 },
    { cy: 236 },
    { cy: 326 },
  ].map((s, i) => ({ ...s, id: `slave${i}`, cx: 76, label: `SLAVE ${i + 1}`, sub: "ESP32" }));
  const masterCx = 372;
  const masterCy = 191;
  const dashCx = 624;
  const dashCy = 191;

  // Edge anchor points for the converging links.
  const slaveOut = (s) => ({ x: s.cx + SLAVE_W / 2, y: s.cy });
  const masterIn = { x: masterCx - MASTER_W / 2, y: masterCy };
  const masterOut = { x: masterCx + MASTER_W / 2, y: masterCy };
  const dashIn = { x: dashCx - DASH_W / 2, y: dashCy };

  return (
    <div className="mx-auto w-full max-w-[560px] animate-[float_6s_ease-in-out_infinite]">
      <style>{`
        @keyframes float { 0%, 100% { transform: translateY(0); } 50% { transform: translateY(-10px); } }
      `}</style>
      <svg
        viewBox="0 0 700 382"
        className="w-full h-auto"
        role="img"
        aria-label="Animated diagram of the wireless battery management system: four wireless slave ESP32 nodes streaming live data into a single master ESP32 hub, which forwards it to a user dashboard."
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

        {/* Links: slave -> master (converging) and master -> dashboard. */}
        {slaves.map((s, i) => {
          const a = slaveOut(s);
          const path = `M ${a.x} ${a.y} L ${masterIn.x} ${masterIn.y}`;
          return (
            <g key={`link-${i}`}>
              <line x1={a.x} y1={a.y} x2={masterIn.x} y2={masterIn.y} stroke="#cbd5e1" strokeWidth="1.5" />
              <circle r="4.5" fill="#0ea5e9" filter="url(#glow)">
                <animateMotion dur="3.4s" begin={`${i * 0.55}s`} repeatCount="indefinite" path={path} />
              </circle>
            </g>
          );
        })}
        {(() => {
          const path = `M ${masterOut.x} ${masterOut.y} L ${dashIn.x} ${dashIn.y}`;
          return (
            <g key="link-dash">
              <line x1={masterOut.x} y1={masterOut.y} x2={dashIn.x} y2={dashIn.y} stroke="#cbd5e1" strokeWidth="1.5" />
              {[0, 1.7].map((delay, j) => (
                <circle key={j} r="4.5" fill="#0ea5e9" filter="url(#glow)">
                  <animateMotion dur="3.4s" begin={`${delay}s`} repeatCount="indefinite" path={path} />
                </circle>
              ))}
            </g>
          );
        })()}

        {/* Slave nodes */}
        {slaves.map((s) => {
          const x = s.cx - SLAVE_W / 2;
          const top = s.cy - SLAVE_H / 2;
          return (
            <g key={s.id}>
              <rect x={x} y={top} width={SLAVE_W} height={SLAVE_H} rx="13" fill="url(#nodeFill)" stroke="#d3dbe6" strokeWidth="1.4" filter="url(#soft)" />
              <circle cx={x + SLAVE_W - 12} cy={top + 12} r="3" fill="#22c55e">
                <animate attributeName="opacity" values="1;0.25;1" dur="1.6s" begin={`${s.cy * 0.004}s`} repeatCount="indefinite" />
              </circle>
              <Glyph id="slave" cx={s.cx - 22} cy={s.cy} />
              <text x={s.cx + 6} y={s.cy - 3} textAnchor="middle" fontSize="12" fontWeight="700" fill="#0f172a" fontFamily="ui-sans-serif, system-ui">{s.label}</text>
              <text x={s.cx + 6} y={s.cy + 11} textAnchor="middle" fontSize="8.5" fontWeight="500" fill="#94a3b8" fontFamily="ui-sans-serif, system-ui">{s.sub}</text>
            </g>
          );
        })}

        {/* Master hub */}
        <g>
          <circle cx={masterCx} cy={masterCy} r="60" fill="url(#hubGlow)">
            <animate attributeName="r" values="50;64;50" dur="3s" repeatCount="indefinite" />
          </circle>
          <rect x={masterCx - MASTER_W / 2} y={masterCy - MASTER_H / 2} width={MASTER_W} height={MASTER_H} rx="15" fill="url(#nodeFill)" stroke="#0891b2" strokeWidth="2" filter="url(#soft)" />
          <circle cx={masterCx + MASTER_W / 2 - 13} cy={masterCy - MASTER_H / 2 + 13} r="3" fill="#22c55e">
            <animate attributeName="opacity" values="1;0.25;1" dur="1.6s" repeatCount="indefinite" />
          </circle>
          <Glyph id="master" cx={masterCx} cy={masterCy - 8} />
          <text x={masterCx} y={masterCy + 25} textAnchor="middle" fontSize="12" fontWeight="700" fill="#0f172a" fontFamily="ui-sans-serif, system-ui">MASTER</text>
          <text x={masterCx} y={masterCy + 38} textAnchor="middle" fontSize="8.5" fontWeight="500" fill="#94a3b8" fontFamily="ui-sans-serif, system-ui">ESP32 hub</text>
        </g>

        {/* Dashboard */}
        <g>
          <rect x={dashCx - DASH_W / 2} y={dashCy - DASH_H / 2} width={DASH_W} height={DASH_H} rx="15" fill="url(#nodeFill)" stroke="#d3dbe6" strokeWidth="1.4" filter="url(#soft)" />
          <circle cx={dashCx + DASH_W / 2 - 13} cy={dashCy - DASH_H / 2 + 13} r="3" fill="#22c55e">
            <animate attributeName="opacity" values="1;0.25;1" dur="1.6s" begin="0.4s" repeatCount="indefinite" />
          </circle>
          <Glyph id="dash" cx={dashCx} cy={dashCy - 6} />
          <text x={dashCx} y={dashCy + 24} textAnchor="middle" fontSize="12" fontWeight="700" fill="#0f172a" fontFamily="ui-sans-serif, system-ui">DASHBOARD</text>
          <text x={dashCx} y={dashCy + 37} textAnchor="middle" fontSize="8.5" fontWeight="500" fill="#94a3b8" fontFamily="ui-sans-serif, system-ui">Users</text>
        </g>
      </svg>
    </div>
  );
}

/* Per-stage monoline icon, centered at (cx, cy). */
function Glyph({ id, cx, cy }) {
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
