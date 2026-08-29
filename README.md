# Hell Run GOAP

Data-authored Goal-Oriented Action Planning for Unreal Engine, with runtime planning, world-state management, simulation/debug support, and an editor graph for authoring GOAP domains.

## Features

- Data-authored GOAP domains with facts, goals, actions, and conditions
- Runtime planning through `UGOAPBrainComponent`
- Agent, squad, and shared world-state support
- Blueprint-callable fact setters/getters for bool, float, vector, object, and generic values
- Explicit replanning and plan/action change events
- Sensors and action tasks
- Planning subsystem and world-state subsystem
- Runtime simulation and debug snapshots
- Editor graph, asset factory, and custom domain editor
- Runtime test coverage

## Modules

- `HellRunGOAP` — Runtime
- `HellRunGOAPEditor` — Editor

## Basic setup

1. Copy the plugin into your project's `Plugins` directory.
2. Enable **Hell Run GOAP** in Unreal Editor.
3. Create a GOAP Domain asset.
4. Add `GOAPBrainComponent` to an AI-controlled actor.
5. Assign the Domain and populate world-state facts from gameplay or sensors.

The brain component can automatically start logic, request replans, expose plan/action change events, and provide debug snapshots for tooling.

## Status

Version 1.0.0. The plugin descriptor currently marks the project as beta.
