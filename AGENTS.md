# AGENTS.md

## Project state

This is a greenfield project — no application code, build tooling, or manifests yet.
The only infrastructure is the `.claude/skills/` framework (superpowers-zh, 20 skills).

## Key files

- `CLAUDE.md` — skill framework rules and the full skill catalog. Read it at session start.
- `.claude/skills/*/SKILL.md` — individual skill definitions. Use the `Skill` tool to load them; do NOT read SKILL.md files directly.

## Workflow rules (from CLAUDE.md)

1. Before any creative work, check if a matching skill exists (even low-probability matches).
2. Design before coding — use `brainstorming` skill for requirements analysis.
3. Write tests before implementation (TDD).
4. Run verification before claiming completion.

## Language

The project uses Chinese (zh-CN) for documentation and communication. Skills are named in English but documented in Chinese.

## When you build something here

Update this file once the project has real structure — manifests, entrypoints, test commands, CI, conventions.
