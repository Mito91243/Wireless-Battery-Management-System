export default function InputAiButton({ inputText }) {
  return (
    <div className="bg-white rounded-xl shadow-lg p-6 max-w-4xl mx-auto">
      <h3 className="text-xl font-semibold text-gray-800 mb-4">Send Message</h3>
      <div className="flex gap-3">
        <input
          type="text"
          value={inputText}
          onChange={(inputText = e.target.value)}
          onClick={(e) => e.key === "Enter" && handleSend()}
          placeholder="Type your message here..."
          className="flex-1 px-4 py-3 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent outline-none"
        />
        <button
          onClick={handleSend}
          disabled={!inputText.trim()}
          className={`px-6 py-3 rounded-lg font-medium text-white transition-all duration-200 ${
            inputText.trim()
              ? "bg-green-600 hover:bg-green-700"
              : "bg-gray-400 cursor-not-allowed"
          }`}
        >
          Send
        </button>
      </div>
    </div>
  );
}
