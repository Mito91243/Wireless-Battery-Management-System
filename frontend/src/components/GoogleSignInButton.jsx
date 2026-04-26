import { useEffect, useRef, useState } from "react";
import { useAuth } from "../context/AuthContext";

const GOOGLE_CLIENT_ID = import.meta.env.VITE_GOOGLE_CLIENT_ID;

export default function GoogleSignInButton({ onSuccess, onError, text = "signin_with" }) {
  const { loginWithGoogle } = useAuth();
  const containerRef = useRef(null);
  const [error, setError] = useState("");

  useEffect(() => {
    if (!GOOGLE_CLIENT_ID) {
      setError("Google sign-in is not configured (missing VITE_GOOGLE_CLIENT_ID)");
      return;
    }

    let cancelled = false;
    let intervalId;

    const initialize = () => {
      if (cancelled || !window.google?.accounts?.id || !containerRef.current) return false;

      window.google.accounts.id.initialize({
        client_id: GOOGLE_CLIENT_ID,
        callback: async (response) => {
          try {
            await loginWithGoogle(response.credential);
            onSuccess?.();
          } catch (err) {
            const message = err.message || "Google sign-in failed";
            setError(message);
            onError?.(message);
          }
        },
      });

      window.google.accounts.id.renderButton(containerRef.current, {
        theme: "outline",
        size: "large",
        text,
        width: containerRef.current.offsetWidth || 320,
      });
      return true;
    };

    if (!initialize()) {
      intervalId = setInterval(() => {
        if (initialize()) clearInterval(intervalId);
      }, 100);
    }

    return () => {
      cancelled = true;
      if (intervalId) clearInterval(intervalId);
    };
  }, [loginWithGoogle, onSuccess, onError, text]);

  return (
    <div className="w-full">
      <div ref={containerRef} className="flex justify-center" />
      {error && <p className="mt-2 text-sm text-red-600">{error}</p>}
    </div>
  );
}
