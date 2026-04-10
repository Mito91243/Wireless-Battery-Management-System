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
      <section className="py-16 sm:py-20 px-4 sm:px-6 lg:px-8 bg-gray-50">
        <div className="max-w-6xl mx-auto text-center">
          <h1 className="text-3xl sm:text-4xl md:text-6xl font-bold text-gray-900 mb-6 break-words">
            Wireless <span className="text-blue-600">Battery Management System</span>
          </h1>
          <p className="text-base sm:text-lg md:text-xl text-gray-600 max-w-4xl mx-auto mb-10 px-2">
            My graduation project: a next-generation BMS that uses wireless communication
            for safer, smarter, and more efficient battery management.
          </p>

          <div className="flex flex-col sm:flex-row gap-4 justify-center items-center">
            <Link to={isAuthenticated ? "/dashboard" : "/login"}>
              <button className="px-8 py-3 bg-gray-900 text-white font-semibold rounded-lg hover:bg-gray-800 focus:outline-none focus:ring-2 focus:ring-gray-500 focus:ring-offset-2 transition duration-200">
                View Prototype
              </button>
            </Link>
            <Link to="/documentation">
              <button className="px-8 py-3 bg-white text-gray-700 font-semibold rounded-lg border border-gray-300 hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-gray-500 focus:ring-offset-2 transition duration-200">
                Learn More
              </button>
            </Link>
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
