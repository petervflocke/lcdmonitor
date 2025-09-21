## Overview

This document provides the high-level guardrails for Codex when collaborating on the lcdmonitor project. Detailed plans, backlog, and process notes live in:

- `PROJECT_PLAN.md` – phase roadmap and milestones.
- `TASKS.md` – current actionable work items.
- `config.toml` – operational defaults (models, profiles, branch prefixes, validation suites).
- `docs/codex-playbook.md` – project-specific working agreement.
- `docs/codex-howto.md` – reusable guidance for future Codex-enabled projects.

## Goals & ownership

- Build and maintain a two-part system: Arduino Nano UI and Python daemon, shipped incrementally with high reliability and security.
- Peter is the human maintainer; Codex acts as implementation partner, reviewer, and documentation assistant.
- For the full breakdown of phases and feature scope, see `PROJECT_PLAN.md` (source of truth to avoid duplication here).

## Tooling & environments

- Arduino development runs through VS Code with the PlatformIO extension. The GUI shortcuts are the primary workflow; CLI equivalents exist in the Makefile but may be skipped when operating inside VS Code.
- Python stack: Python 3.12, `uv`, pytest, Ruff, mypy (strict). Prefer Make targets (`make fmt`, `make lint`, etc.) over raw commands.
- Hardware validation (uploading firmware, serial monitoring, end-to-end tests) must be performed manually on the target workstation. When Codex cannot execute these steps, it must flag them explicitly in status updates, and Peter confirms results or records outstanding tests.

## Process expectations

- Begin each session by reviewing `TASKS.md` and aligning with the entries in `config.toml` and the Codex playbook.
- Use Codex profiles `readonly` (discovery/review) and `dev` (implementation) as described in `config.toml`. Mention profile switches in session notes when they affect command execution.
- Keep `TASKS.md` updated as work progresses; defer to the playbook for intake format (`Goal / Constraints / Definition of done`).
- Record any skipped or pending validations (especially hardware checks) and request manual confirmation.

## Version control & CI

- Follow branch prefixes and PR policy defined in `config.toml` and `docs/codex-playbook.md` (`feature/`, `fix/`, `chore/`; always merge via PR with tests/docs updated).
- CI requirements, logging standards, and security posture are detailed in the playbook and `PROJECT_PLAN.md`. Avoid repeating them here to reduce drift.

## Escalations & guardrails

- Respect the protected paths listed in `config.toml` and seek explicit confirmation before destructive actions.
- Do not introduce new dependencies, schema changes, or external calls without following the approval rules in the playbook.
- When hardware testing cannot be automated, mark tasks as “hardware verification pending” in `TASKS.md` and the final handoff message.

## Reference entry points

- `README.md` – onboarding, repository layout, setup commands.
- `docs/wiring.md` – hardware connections.
- `docs/adr/` – architectural decisions and protocol notes.
- `docs/deployment-systemd.md` – server deployment guidance.

Treat this file as the lightweight entry point while keeping authoritative details in the dedicated documents called out above.
