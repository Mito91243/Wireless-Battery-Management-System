import "./App.css";
import Dashboard from "./pages/Dashboard";
import { Route, Routes } from "react-router-dom";
import Header from "./components/Header";
import SignUpComponent from "./pages/Signup";
import SignInComponent from "./pages/Login";
import AboutPage from "./pages/About";
import FeaturesPage from "./pages/Features";
import HomePage from "./pages/Home";
function App() {
  return (
    <>
      <Header />
      <Routes>
        <Route path="/dashboard" element={<Dashboard />}></Route>
        <Route path="/signup" element={<SignUpComponent />}></Route>
        <Route path="/login" element={<SignInComponent />}></Route>
        <Route path="/about" element={<AboutPage />}></Route>
        <Route path="/features" element={<FeaturesPage />}></Route>
        <Route path="/" element={<HomePage />}></Route>
        <Route path="*" element={<FeaturesPage />}></Route>
      </Routes>
    </>
  );
}

export default App;
