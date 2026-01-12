\# Architecture Ledger



This document records \*\*settled architectural decisions\*\* for the project.



It is not a design discussion log.

It is not a task list.

It is the source of truth for \*why\* systems are structured the way they are.



If a decision affects ownership, ordering, data shape, or system boundaries,

it belongs here.



---



\## Rules



\- Only write \*\*decisions that are finalized\*\*

\- One entry per decision

\- No brainstorming or speculation

\- Prefer clarity over completeness

\- If it’s confusing without context, add a short reason — not a story



---



\## How to Use This File



When returning to the project after time away:

1\. Skim entry titles to reload context

2\. Read the most recent decisions first

3\. Use “Where enforced” to jump into code



When stopping work after solving a non-obvious problem:

1\. Add a new entry

2\. Leave a breadcrumb comment in code if needed



---



\## Decisions



\### \[2026-01-10] Direction enum and CouplerState struct



\*\*Decision\*\*  

Direction is represented by an enum (`E\_Direction`) rather than a boolean.  

Rolling stock position/state relative to track is represented by a `CouplerState` struct containing:

\- `TrackID` (Name)

\- `DistanceAlongTrack` (float)

\- `Direction` (E\_Direction)



\*\*Reason\*\*  

A boolean direction flag is ambiguous and does not communicate intent as systems grow (solver order, reversal, AI routing, etc.).  

An enum is self-documenting, safer to pass between systems, and extensible.  

Grouping track-relative data into a struct makes ownership and data flow explicit between rolling stock, trains, and track systems.



\*\*Where enforced\*\*  

\- `E\_Direction` enum definition  

\- `FCouplerState` struct  

\- Rolling stock solver  

\- Track-relative positioning logic



\*\*Notes\*\*  

Do not reintroduce direction booleans.  

If additional direction states are required in the future, extend `E\_Direction` rather than adding flags.





