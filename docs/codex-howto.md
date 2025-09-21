# Codex Collaboration How-To

A reusable playbook for future projects that tap Codex as a programming partner. Adapt these steps to each repository and keep them under version control so every session starts from the same expectations.

## 1. Kickoff checklist
1. **Define scope** – Capture project goals, phase breakdown, and success metrics in `PROJECT_PLAN.md` (or equivalent).
2. **Write the agent brief** – Create `AGENTS.md` with guardrails, tooling, CI expectations, and any compliance constraints.
3. **Seed the backlog** – Maintain `TASKS.md` (or `tasks/*.md`) with the active checklist; Codex reads this on every engagement.
4. **Add a configuration file** – Provide `config.toml` describing default models, profiles, branch strategy, required commands, and protected paths.
5. **Document the environment** – List key commands (Make targets, scripts) and credentials required for local vs. remote operation.

## 2. Session workflow
- Start in a safe profile (`readonly`) to gather context; switch to `dev` before running commands or editing files.
- Frame each request as **Goal / Constraints / Definition of done**. Mention validation expectations (tests, builds) explicitly.
- Use file + line references instead of pasting long snippets (e.g., `src/service.py:42`).
- Keep Auto Context on by default but close irrelevant tabs to avoid prompt bloat. Toggle off when discussing unrelated work.

## 3. Branching & CI discipline
- Use short-lived branches (`feature/`, `fix/`, `chore/`). Keep `main` deployable.
- Require PRs for all changes; enforce lint/type/test checks locally and in CI (`make fmt`, `make lint`, `make type`, `make test`, plus project-specific suites).
- Provide rollback guidance and risk notes in each PR description.
- Update docs and sample configs when behavior changes to prevent drift.

## 4. Approvals and sandboxing
- Prefer documented Make targets. If a custom command is necessary, explain why.
- Escalate commands that need elevated permissions or break out of the sandbox; include a one-line justification.
- Treat destructive commands as last resort. Seek explicit confirmation from the human partner before proceeding.
- Record pending approvals or blocked actions in the handoff summary so the next session knows what to revisit.

## 5. Documentation & context hygiene
- Keep `README` or `/docs` aligned with implementation status.
- Log architecture decisions in `docs/adr/NNNN-name.md`; reference them from code comments where it helps future readers.
- Summarize progress, remaining work, and tests executed in the wrap-up message using the status template from `config.toml`.
- Archive retrospectives and lessons learned in a dated journal (`docs/journal/`). Fold actionable items into the playbook or config files.

## 6. Handoff template
```
Branch: <name>
Work done: <bullets>
Tests: <fmt/lint/type/test/hardware>
Docs: <updated files or N/A>
Next steps: <what remains, approvals needed>
Risks: <open questions, blockers>
```
Use this when pausing work or opening a PR so teammates and future Codex sessions can ramp quickly.

## 7. Model selection guide
- **Small/medium models** – Day-to-day implementation, quick fixes, documentation edits.
- **Large models** – Architecture, audits, large refactors, or when context is very large.
- Document the default in `config.toml` and note when to upsize/downsize.

## 8. Continuous improvement
- After each sprint or major milestone, run a mini-retro: what slowed us down, what worked well, and what should change.
- Update this guide, the project-specific playbook, and `config.toml` with any new rules, commands, or gotchas.
- Revisit guardrails quarterly to ensure they still reflect risk appetite (e.g., new protected paths, additional tests).

Keep this how-to lightweight but authoritative. Every new project should clone it, tweak the specifics, and commit the customized version so Codex always operates with clear guidance.
