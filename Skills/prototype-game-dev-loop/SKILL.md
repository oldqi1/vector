---
name: prototype-game-dev-loop
description: Run a disciplined, multi-turn game-prototype development loop from design-scope alignment through implementation, diagnostics, validation, player acceptance, progress reporting, and optional Git checkpoints. Use for Unreal, Unity, or custom-engine prototype work when Codex edits the project while the user may own compilation or in-editor playtests; also use when the user asks what to build next, requests a producer-level scope review, pastes gameplay logs, asks how to test a feature, or wants the current prototype uploaded to Git.
---

# Prototype Game Dev Loop

Keep design intent, repository state, mathematical correctness, and observed game feel synchronized across long conversations.

## Establish the source of truth

1. Read repository instructions and the smallest relevant set of design, status, and test documents.
2. Inspect the current implementation and working tree before relying on status prose.
3. Separate these categories explicitly:
   - player-tested and accepted;
   - implemented and mechanically validated, awaiting editor/playtest acceptance;
   - designed but not implemented;
   - long-term or explicitly out of prototype scope.
4. Give the latest explicit user decision precedence over older proposals. Preserve the original design baseline unless the user intentionally changes it.
5. Flag conflicts between a new suggestion and the frozen prototype scope. Do not silently turn a temporary idea into the product direction.

## Work one evidence-bearing slice at a time

Choose the smallest end-to-end slice that answers a gameplay question. Prefer a complete interaction with feedback and acceptance criteria over several disconnected systems.

For each slice:

1. State the intended player-visible outcome.
2. Inspect the relevant code, data, maps, scripts, and existing tests.
3. Make scoped changes while preserving unrelated and pre-existing worktree changes.
4. Add diagnostics where visual judgment alone is ambiguous.
5. Validate at the lowest deterministic layer available.
6. Provide exact editor/playtest steps and pass/fail criteria.
7. Wait for player evidence when game feel or integration cannot be established offline.

When the user says to continue for an extended period, keep moving through safe in-scope slices. Send concise progress updates and do not wait for confirmation between reversible implementation steps.

## Use the validation ladder

Treat the following as distinct evidence levels; never collapse them into “done”:

1. **Static integrity**: syntax-adjacent scans, includes, declarations/definitions, input wiring, serialization-sensitive declarations, formatting checks.
2. **Pure model validation**: unit tests or offline simulations for formulas, invariants, thresholds, and state transitions.
3. **Build validation**: compiler and reflection/header-tool success.
4. **Automation validation**: registered test suite runs with zero failures.
5. **Editor integration**: maps, spawned actors, input, HUD, assets, navigation, collision channels, and runtime lifecycle work in the editor.
6. **Gameplay acceptance**: the player can see, predict, reproduce, and enjoy the intended behavior.

Report only the levels actually observed. If the user compiles locally, do not claim build success. Perform static and offline checks, then hand over a compact build-and-test checklist.

Count registered tests using anchored registration patterns rather than raw text matches so comments do not inflate the result. For example, count lines beginning with the engine's test-registration macro.

## Design useful diagnostics

Log decisions and invariants, not every frame by default. Include enough fields to reconstruct the result:

- actors and action/mode;
- input and output velocities;
- relevant mass or modifier class;
- damage or state delta;
- conserved quantity before and after;
- explicit `PASS`/`FAIL` for invariants;
- rejection, cancellation, cooldown, and cleanup reasons.

Throttle continuous diagnostics. Pair each expected log with a visible gameplay criterion so passing logs cannot hide poor feedback or feel.

## Hand off tests as player actions

Write acceptance steps in this order:

1. setup or fixture;
2. exact input sequence;
3. visible result;
4. expected diagnostic evidence;
5. failure symptoms and what they imply.

Use concrete thresholds when they exist. Distinguish “must pass” correctness from tunable feel. Invite the user to paste the smallest relevant log block when a test fails.

For Unreal projects with reflection-visible C++ changes, call out the need to close the editor or disable Live Coding before a full build. Prefer Session Frontend for Automation when command-line editor infrastructure is known to be unreliable. Never launch a heavyweight engine build when the user has said they will compile.

## Review gameplay as a producer

Evaluate a mechanic against the prototype's core promise before expanding it:

- Does it create a distinct decision rather than duplicate another tool?
- Can the player intentionally set it up and predict the result?
- Does it reuse the shared physics/combat model?
- Is the feedback immediate and readable?
- Does it improve the encounter loop, or only add spectacle?
- Is it inside the current milestone?

When asked what to do next, finish unresolved acceptance risks first. Then choose the smallest loop that gives existing mechanics a purpose. Keep permanent progression, procedural generation, content scale, and polish out until their prerequisite gameplay claim is proven.

## Maintain an honest progress ledger

Summarize progress with four sections:

1. confirmed in play;
2. implemented but awaiting integration or feel testing;
3. not started or out of scope;
4. next recommended validation or slice.

Prefer evidence over percentages. Correct stale documents when implementation or user decisions have changed, but preserve clearly historical records as historical evidence.

## Create Git checkpoints only when authorized

Uploading, committing, and pushing require an explicit user request.

When authorized:

1. inspect repository instructions, status, branch, remotes, and recent history;
2. identify local/editor/tool metadata that should remain ignored;
3. preserve all user work and avoid cleanup, reset, or checkout operations;
4. run the relevant deterministic checks;
5. stage the intended milestone and inspect the staged status/stat;
6. run a staged whitespace/error check;
7. commit with a milestone-level message;
8. push the requested branch;
9. verify local `HEAD` equals the remote-tracking ref;
10. report commit id, branch, verification evidence, and any intentionally untracked files.

Do not claim a clean worktree until verified after the push.

## Communication contract

Lead with the outcome. Keep commentary brief but frequent during tool work. In the final handoff, include only:

- what changed or what was concluded;
- evidence obtained;
- what the user should test next;
- what remains uncertain;
- the safest next step.

Use the user's vocabulary and skill level. Correct earlier mistakes directly, especially test counts, completion claims, or scope drift.
