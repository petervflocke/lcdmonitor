# Codex Playbook (lcdmonitor)

This playbook captures the working agreement between Peter and Codex for the lcdmonitor project. It complements `AGENTS.md`, `PROJECT_PLAN.md`, `TASKS.md`, and `config.toml`.

## 1. Engagement model
- **Profiles** – Start in `readonly` for discovery; switch to `dev` before editing files or running commands. Record profile changes in the session notes when relevant.
- **Model sizing** – Default to `gpt5-codex-medium`. Upsize to `gpt5-codex-large` for architecture conversations, multi-file refactors, or when the agent struggles with context size.
- **Auto Context** – Keep enabled for day-to-day work; close unused tabs to reduce noise. Disable temporarily when discussing unrelated projects or sensitive data.

## 2. Task intake
- Reference `TASKS.md` or open a new checkbox list before starting implementation.
- Format requests as `Goal / Constraints / Definition of done` when possible.
- Point to files and line numbers (e.g., `server/src/main.py:120`) instead of pasting large snippets.
- Call out mandatory validations (e.g., "run `make test`") so Codex executes or explicitly defers them.

## 3. Branching & PR flow
- Create short-lived branches with prefixes from `config.toml` (`feature/`, `fix/`, `chore/`).
- Keep `main` green; never land changes without passing formatter, lint, type, and test steps.
- Each PR must include:
  - Updated tests/docs when behavior changes.
  - Summary of validations (`make fmt`, `make lint`, `make type`, `make test`, hardware checks).
  - Risk callouts and rollback guidance where applicable.

## 4. Command guardrails
- Prefer `make` targets over raw commands. If ad-hoc commands are needed, explain why.
- Hardware commands (`make arduino-upload`, `make e2e`) run only from a trusted local shell.
- Escalate when a command requires elevated permissions or hits sandbox boundaries; include a one-line justification per the CLI rules.
- Avoid destructive operations (`rm`, `git reset`) unless explicitly requested and documented.

## 5. Validation checklist
Run the following before handoff or PR unless the task scope forbids it:
1. `make fmt`
2. `make lint`
3. `make type`
4. `make test`
5. Arduino-specific checks when touching firmware: `make arduino-build`, optionally `make e2e PORT=/dev/ttyACM0`
6. Security sweeps as needed (`make audit`, manual review of secrets)

Document any skipped item with rationale and next steps.

## 6. Documentation hygiene
- Update `TASKS.md` as progress is made; mark completed boxes and add new ones for follow-up work.
- Keep README and deployment docs aligned with behavior changes.
- Capture architectural decisions and protocol adjustments in `docs/adr/` (increment the ADR index and link to relevant commits/PRs).
- Use the status template from `config.toml` for concise handoffs.

## 7. Handoff expectations
- Summarize the current branch, outstanding work, and validations run.
- Mention open approvals or environment requirements (e.g., "needs Arduino attached on next run").
- Flag TODO comments introduced in the code and align them with `TASKS.md` items.
- Offer next-step suggestions (tests to run, docs to update) to help the next contributor ramp quickly.

## 8. Retrospectives & continuous improvement
- After each milestone, log quick highlights and pain points in `TASKS.md` or a dated note. Fold actionable items back into this playbook or `config.toml`.
- Review guardrails, preferred commands, and validation suites quarterly; adjust as the project evolves.
- Track any friction with profiles, approvals, or CI so we can streamline the workflow in subsequent iterations.

## 9. Contact map
- Primary maintainer: Peter (`@` on GitHub).
- Codex responsibilities: implementation support, reviews, documentation updates, and automation suggestions.
- Escalation: for blockers outside Codex’s control (network isolation, missing hardware), annotate the task and request manual intervention.

This document is living—update it alongside process changes so every collaboration session starts from an accurate baseline.
