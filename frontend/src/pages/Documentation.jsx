import { Link } from "react-router-dom";
import {
  UserPlus,
  Settings,
  Hash,
  Activity,
  BookOpen,
  ArrowRight,
  CheckCircle,
} from "lucide-react";

export default function DocumentationPage() {
  const steps = [
    {
      number: 1,
      icon: <UserPlus className="w-6 h-6 text-white" />,
      color: "bg-blue-600",
      title: "Sign Up for an Account",
      description:
        "Create your free WBMS account to access the dashboard. Head to the Sign Up page and enter your credentials. Once registered, log in to continue.",
      action: { label: "Go to Sign Up", to: "/signup" },
    },
    {
      number: 2,
      icon: <Settings className="w-6 h-6 text-white" />,
      color: "bg-indigo-600",
      title: "Choose Your Battery Configuration",
      description:
        "From the dashboard, add a new battery pack and configure its topology. Select the number of cells in series (S) and parallel (P) - for example, 13S4P - so the system knows the layout of your pack.",
    },
    {
      number: 3,
      icon: <Hash className="w-6 h-6 text-white" />,
      color: "bg-sky-600",
      title: "Input the Battery ID",
      description:
        "Enter the unique identifier printed on the physical WBMS module attached to your battery. This pack identifier links your hardware to the dashboard so data is routed to the right battery.",
    },
    {
      number: 4,
      icon: <Activity className="w-6 h-6 text-white" />,
      color: "bg-emerald-600",
      title: "Start Monitoring",
      description:
        "Power on the battery module. Your dashboard will begin receiving real-time voltage, current, temperature, and state-of-charge data. View live charts, cell balance, and alerts at any time.",
      action: { label: "Open Dashboard", to: "/dashboard" },
    },
  ];

  const tips = [
    "Make sure the WBMS module is powered and within wireless range of the receiver.",
    "Double-check the series/parallel counts match your physical pack before saving.",
    "The pack identifier is case-sensitive - enter it exactly as printed on the module.",
    "You can add multiple battery packs to a single account and switch between them.",
  ];

  return (
    <div className="min-h-screen bg-white">
      {/* Hero */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8 bg-gradient-to-b from-blue-50 to-white">
        <div className="max-w-4xl mx-auto text-center">
          <div className="inline-flex items-center justify-center w-14 h-14 sm:w-16 sm:h-16 rounded-2xl bg-blue-600 mb-5 sm:mb-6">
            <BookOpen className="w-7 h-7 sm:w-8 sm:h-8 text-white" />
          </div>
          <h1 className="text-3xl sm:text-4xl md:text-5xl font-bold text-gray-900 mb-4 break-words">
            Getting Started with{" "}
            <span className="text-blue-600">WBMS</span>
          </h1>
          <p className="text-base sm:text-lg md:text-xl text-gray-600 max-w-2xl mx-auto">
            A quick four-step guide to setting up your Wireless Battery
            Management System and starting real-time monitoring.
          </p>
        </div>
      </section>

      {/* Steps */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8">
        <div className="max-w-3xl mx-auto">
          <ol className="space-y-6 sm:space-y-8">
            {steps.map((step) => (
              <li
                key={step.number}
                className="relative bg-white rounded-2xl border border-gray-200 shadow-sm p-5 sm:p-8 hover:shadow-md transition-shadow"
              >
                <div className="flex flex-col sm:flex-row sm:items-start gap-4 sm:gap-6">
                  <div className="flex items-center gap-3 sm:block sm:flex-shrink-0">
                    <div
                      className={`w-12 h-12 sm:w-14 sm:h-14 ${step.color} rounded-xl flex items-center justify-center shadow-sm`}
                    >
                      {step.icon}
                    </div>
                    <span className="sm:hidden text-sm font-semibold text-gray-500">
                      Step {step.number}
                    </span>
                  </div>
                  <div className="flex-1 min-w-0">
                    <span className="hidden sm:inline-block text-xs font-semibold uppercase tracking-wide text-gray-500 mb-1">
                      Step {step.number}
                    </span>
                    <h2 className="text-lg sm:text-2xl font-bold text-gray-900 mb-2 break-words">
                      {step.title}
                    </h2>
                    <p className="text-sm sm:text-base text-gray-600 leading-relaxed break-words">
                      {step.description}
                    </p>
                    {step.action && (
                      <Link
                        to={step.action.to}
                        className="inline-flex items-center gap-2 mt-4 text-sm sm:text-base font-semibold text-blue-600 hover:text-blue-700"
                      >
                        {step.action.label}
                        <ArrowRight className="w-4 h-4" />
                      </Link>
                    )}
                  </div>
                </div>
              </li>
            ))}
          </ol>
        </div>
      </section>

      {/* Tips */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8 bg-gray-50">
        <div className="max-w-3xl mx-auto">
          <h2 className="text-2xl sm:text-3xl font-bold text-gray-900 mb-6 sm:mb-8 text-center">
            Helpful Tips
          </h2>
          <ul className="space-y-3 sm:space-y-4">
            {tips.map((tip, idx) => (
              <li
                key={idx}
                className="flex items-start gap-3 bg-white rounded-xl p-4 sm:p-5 border border-gray-200"
              >
                <CheckCircle className="w-5 h-5 text-green-500 flex-shrink-0 mt-0.5" />
                <span className="text-sm sm:text-base text-gray-700 leading-relaxed break-words">
                  {tip}
                </span>
              </li>
            ))}
          </ul>
        </div>
      </section>

      {/* CTA */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8">
        <div className="max-w-2xl mx-auto text-center">
          <h2 className="text-2xl sm:text-3xl font-bold text-gray-900 mb-4">
            Ready to monitor your battery?
          </h2>
          <p className="text-base sm:text-lg text-gray-600 mb-6 sm:mb-8">
            Sign up or log in to start tracking your pack in real time.
          </p>
          <div className="flex flex-col sm:flex-row gap-3 sm:gap-4 justify-center">
            <Link
              to="/signup"
              className="px-6 sm:px-8 py-3 bg-blue-600 text-white font-semibold rounded-lg hover:bg-blue-700 transition duration-200"
            >
              Create Account
            </Link>
            <Link
              to="/login"
              className="px-6 sm:px-8 py-3 bg-white text-gray-700 font-semibold rounded-lg border border-gray-300 hover:bg-gray-50 transition duration-200"
            >
              Log In
            </Link>
          </div>
        </div>
      </section>
    </div>
  );
}
