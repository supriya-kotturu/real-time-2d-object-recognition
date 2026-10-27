// Silhouette's mark (D-28): the task in one picture. A dark object on light paper, its oriented
// bounding box in lime with the overlay's dark halo, and the primary axis in blue.
// Same drawing as app/icon.svg (the favicon).
export function Logo({ className }: { className?: string }) {
  return (
    <svg viewBox="0 0 40 40" className={className} role="img" aria-label="Silhouette logo">
      <rect width="40" height="40" rx="10" fill="#e7f2ea" />
      <g transform="rotate(-28 20 20)">
        <path
          d="M8.5 20c0-4.6 4.4-7.6 10.6-7.6 6.9 0 12.4 3 12.4 7.4 0 4.7-5.4 7.8-11.6 7.8C13.4 27.6 8.5 24.7 8.5 20z"
          fill="#0b0f0c"
        />
        <rect x="7" y="11" width="26" height="18" fill="none" stroke="#0b0f0c" strokeWidth="3.2" />
        <rect x="7" y="11" width="26" height="18" fill="none" stroke="#7cff6b" strokeWidth="1.6" />
        <line x1="5.5" y1="20" x2="34.5" y2="20" stroke="#4d8dff" strokeWidth="2" strokeLinecap="round" />
        <circle cx="20" cy="20" r="2" fill="#7cff6b" stroke="#0b0f0c" strokeWidth="1" />
      </g>
    </svg>
  );
}
