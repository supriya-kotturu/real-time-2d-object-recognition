# Web GUI

Local control panel for the recognition pipeline (decision D-13 in `docs/mentorship/decisions.md`).
Next.js 16 + TypeScript + Tailwind v4 + shadcn/ui (Base UI).

## Run

Two processes: the C++ backend owns the camera and the pipeline; this app only displays and controls it.

```sh
# 1. backend (from the repo root, after `cmake --build --preset debug`)
./out/server.exe 0                     # camera 0
./out/server.exe http://<phone-ip>:8080/video   # IP Webcam stream (D-07); 8080 there is the phone's port
./out/server.exe data/examples         # still images, "Next image" button steps through

# 2. frontend (from web/)
npm install
npm run dev                            # http://localhost:3000
```

If the backend isn't on `127.0.0.1:8080`, set both:

```sh
OBJREC_BACKEND=http://127.0.0.1:8081 NEXT_PUBLIC_STREAM_ORIGIN=http://127.0.0.1:8081 npm run dev
```

## How it talks to the backend

| What | Path | Why |
| --- | --- | --- |
| JSON API | `/api/*` → Next rewrite → `server.exe` | same-origin, no CORS |
| Video | `<img src="http://127.0.0.1:8080/stream?view=…">` | MJPEG; rewrite proxies may buffer multipart |
| Status | poll `/api/status` every 250 ms | a handful of fields; no second streaming channel |

## Styling rule

Long utility lists live as named classes in `src/app/globals.css` (`@layer components`), e.g. `dashboard-shell`.
Generated shadcn components in `src/components/ui/` are left untouched so `npx shadcn add` can update them.
