import "./App.css";
import Dashboard from "./pages/Dashboard";
import { Route, Routes } from "react-router-dom";
import Header from "./components/Header";
import SignUpComponent from "./pages/Signup";
import SignInComponent from "./pages/Login";
import AboutPage from "./pages/About";
import FeaturesPage from "./pages/Features";
import HomePage from "./pages/Home";
import DocumentationPage from "./pages/Documentation";
import ProtectedRoute from "./components/ProtectedRoute";
import { useLocation } from "react-router-dom";

function App() {
  const location = useLocation();
  const isDashboard = location.pathname === "/dashboard";

  return (
    <>
      {!isDashboard && <Header />}
      <Routes>
        <Route element={<ProtectedRoute />}>
          <Route path="/dashboard" element={<Dashboard />} />
        </Route>
        <Route path="/signup" element={<SignUpComponent />} />
        <Route path="/login" element={<SignInComponent />} />
        <Route path="/about" element={<AboutPage />} />
        <Route path="/features" element={<FeaturesPage />} />
        <Route path="/documentation" element={<DocumentationPage />} />
        <Route path="/" element={<HomePage />} />
        <Route path="*" element={<FeaturesPage />} />
      </Routes>
    </>
  );
}

export default App;
