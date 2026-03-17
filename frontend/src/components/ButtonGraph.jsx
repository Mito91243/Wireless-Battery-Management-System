export function ChartBtn(name,description,selectedFile) {
  return (
    <button
      type="button"
      className="group relative bg-gradient-to-r from-purple-600 to-indigo-600 text-white p-6 rounded-xl font-semibold text-lg shadow-lg hover:shadow-xl transform hover:-translate-y-1 transition-all duration-300 overflow-hidden"
      disabled={!selectedFile}
    >
      <div className="absolute inset-0 bg-gradient-to-r from-purple-700 to-indigo-700 opacity-0 group-hover:opacity-100 transition-opacity duration-300"></div>
      <div className="relative flex items-center justify-center space-x-3">
        <span>Create {name}</span>
      </div>
      <div className="relative mt-2 text-sm opacity-90">{description}</div>
    </button>
  );
}
