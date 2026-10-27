# Session 0 - Reading the assignment

Session log `d0f5898e-fe04-4da2-a9a0-87c9878b28e3` · started 2026-10-23

Conversation only: tool calls, build output and system messages are omitted.

## Supriya

can you create a task.md from task.pdf to structure for the agent to access

## Claude

I wrote [task.md](task.md) from the PDF. It has a short goal and constraints table, a one-line pipeline diagram, and Tasks 1–9. Each task is split into what to implement, what to display at runtime, and what goes in the report. After that come the extensions, the report structure, a checklist of required images, and the submission rules.

Things I noticed along the way:
- **`.CMakeLists.txt` is from your last project.** It still says `project(content_based_image_retrieval ...)` and refers to `src/features.cpp`, `src/metrics.cpp` and other files that aren't in this repo. The leading dot in the filename also means CMake won't find it. It needs renaming to `CMakeLists.txt` and updating.
- **Course downloads aren't linked in task.md.** The PDF has links for the instructor's utilities code, the ResNet18 model and the PCA Python script, but they need a Canvas login. Task 9 says to download them manually and points to page 4 of the PDF.
- **Your choices could go in task.md.** Once you decide which stage(s) you'll write from scratch, which embedding option you'll use and what your input setup is, adding them there would keep the agent from guessing.
