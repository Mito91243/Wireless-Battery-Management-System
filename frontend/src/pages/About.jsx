import { Users, Target, Award, Heart } from 'lucide-react';
import { Link } from 'react-router-dom';

export default function AboutPage() {
  const values = [
    {
      icon: Users,
      title: "Teamwork",
      description: "As a team of 4 Electrical Engineering students from Cairo University, collaboration is at the heart of everything we do.",
      color: "text-blue-500"
    },
    {
      icon: Target,
      title: "Innovation",
      description: "We aim to push the boundaries of traditional battery management systems by introducing wireless communication and modern control methods.",
      color: "text-green-500"
    },
    {
      icon: Award,
      title: "Excellence",
      description: "Guided by Dr. Mohamed Taha, we are committed to high academic and technical standards to deliver a reliable and impactful graduation project.",
      color: "text-purple-500"
    },
    {
      icon: Heart,
      title: "Impact",
      description: "Our project addresses the growing demand for safer and more efficient energy storage solutions, contributing to future technologies.",
      color: "text-red-500"
    }
  ];

  return (
    <div className="min-h-screen bg-white overflow-x-hidden">
      {/* Hero Section */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8">
        <div className="max-w-4xl mx-auto text-center">
          <h1 className="text-3xl sm:text-4xl font-bold text-gray-900 mb-6 break-words">
            About Our Project
          </h1>
          <p className="text-base sm:text-lg text-gray-600 leading-relaxed max-w-3xl mx-auto px-2">
            We are a team of Electrical Engineering students from Cairo University, developing a Wireless Battery Management System as our graduation project under the guidance of Dr. Mohamed Taha.
          </p>
        </div>
      </section>

      {/* Our Story Section */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8 bg-gray-50">
        <div className="max-w-4xl mx-auto">
          <h2 className="text-2xl sm:text-3xl font-bold text-gray-900 text-center mb-8 sm:mb-12 break-words">
            Our Story
          </h2>
          <div className="space-y-6 text-sm sm:text-base text-gray-600 leading-relaxed break-words">
            <p>
              The rise of renewable energy and electric vehicles has made batteries more critical than ever. However, traditional wired battery management systems introduce complexity, cost, and safety concerns.
            </p>
            <p>
              To address these challenges, we designed a Wireless Battery Management System that enables real-time monitoring, efficient cell balancing, and improved safety without the need for cumbersome wiring.
            </p>
            <p>
              This project is more than an academic requirement—it is our contribution toward the advancement of energy storage technologies.
            </p>
          </div>
        </div>
      </section>

      {/* Our Values Section */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8">
        <div className="max-w-6xl mx-auto">
          <h2 className="text-2xl sm:text-3xl font-bold text-gray-900 text-center mb-8 sm:mb-12 break-words">
            Our Values
          </h2>
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6 sm:gap-8">
            {values.map((value, index) => {
              const IconComponent = value.icon;
              return (
                <div key={index} className="text-center">
                  <div className={`inline-flex items-center justify-center w-16 h-16 rounded-full bg-gray-50 mb-4 ${value.color}`}>
                    <IconComponent size={32} />
                  </div>
                  <h3 className="text-lg sm:text-xl font-semibold text-gray-900 mb-3 break-words">
                    {value.title}
                  </h3>
                  <p className="text-sm sm:text-base text-gray-600 leading-relaxed break-words">
                    {value.description}
                  </p>
                </div>
              );
            })}
          </div>
        </div>
      </section>

      {/* Our Team Section */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8 bg-gray-50">
        <div className="max-w-4xl mx-auto">
          <h2 className="text-2xl sm:text-3xl font-bold text-gray-900 text-center mb-6 sm:mb-8 break-words">
            Our Team
          </h2>
          <p className="text-base sm:text-lg text-gray-600 leading-relaxed text-center mb-8 sm:mb-12 break-words">
            We are 4 senior students from Cairo University, Electrical Engineering Department, working together with the support and consultation of Dr. Mohamed Taha.
          </p>
          
          {/* Team Quote */}
          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6 sm:p-8 max-w-3xl mx-auto">
            <div className="text-center">
              <svg className="w-8 h-8 text-gray-400 mb-4 mx-auto" fill="currentColor" viewBox="0 0 24 24">
                <path d="M14.017 21v-7.391c0-5.704 3.731-9.57 8.983-10.609l.995 2.151c-2.432.917-3.995 3.638-3.995 5.849h4v10h-9.983zm-14.017 0v-7.391c0-5.704 3.748-9.57 9-10.609l.996 2.151c-2.433.917-3.996 3.638-3.996 5.849h4v10h-10z"/>
              </svg>
              <blockquote className="text-lg sm:text-xl text-gray-700 italic mb-4 leading-relaxed break-words">
                "Innovation is the fuel that powers the future of energy."
              </blockquote>
              <cite className="text-sm sm:text-base text-gray-600 font-medium break-words">
                — Cairo University Graduation Project Team
              </cite>
            </div>
          </div>
        </div>
      </section>

      {/* Call to Action Section */}
      <section className="py-12 sm:py-16 px-4 sm:px-6 lg:px-8">
        <div className="max-w-4xl mx-auto text-center">
          <h2 className="text-2xl sm:text-3xl font-bold text-gray-900 mb-6 break-words">
            Want to Learn More?
          </h2>
          <p className="text-base sm:text-lg text-gray-600 mb-8 max-w-2xl mx-auto break-words">
            Explore our Wireless Battery Management System project and discover how wireless technology can reshape the future of energy storage.
          </p>
          <div className="flex flex-col sm:flex-row gap-4 justify-center">
            <Link
              to="/documentation"
              className="bg-blue-600 text-white px-8 py-3 rounded-md font-medium hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-blue-500 focus:ring-offset-2 transition duration-200 inline-block"
            >
              View Documentation
            </Link>
            <button className="border border-gray-300 text-gray-700 px-8 py-3 rounded-md font-medium hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-gray-500 focus:ring-offset-2 transition duration-200">
              See Prototype
            </button>
          </div>
        </div>
      </section>
    </div>
  );
}
