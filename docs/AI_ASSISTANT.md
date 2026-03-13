# AI Assistant - Editor-side AI Integration

## Architecture Summary

The AI Assistant is an editor-side module that:

1. **Extracts editor context** (map, selection, camera, nearby entities) into structured JSON
2. **Sends context + user prompt** to external AI providers (OpenAI, Gemini) via HTTP REST
3. **Parses structured placement plans** from AI responses
4. **Validates** each placement action against allowed entities and asset catalog
5. **Executes** approved actions through the undo system

### Module Boundaries

- **`radiant/ai_assistant_workbench.cpp`** – Single compilation unit containing:
  - JSON schema types (EditorContext, PlacementPlan, PlacementAction)
  - Context extraction (AIContextCollector scene walker)
  - HTTP transport (QNetworkAccessManager)
  - Provider request building (OpenAI-style messages)
  - Validation and undo-safe execution
  - Qt dock UI

- **`include/ai_assistant.h`** – Public API

### Dependencies

- **Qt5Network** – Async HTTP (QNetworkAccessManager)
- **RapidJSON** – JSON serialization (libs/rapidjson)
- **Existing**: iundo, iselection, ientity, scenelib, map, camwindow

## Phased Implementation Plan

### Milestone 1 (Implemented)

- [x] Qt5Network + async HTTP
- [x] JSON EditorContext and PlacementPlan structs
- [x] AI Assistant dock with provider/model/endpoint fields
- [x] Context extraction (selection, camera, nearby entities)
- [x] Dry-run: parse plan, list actions, Apply Selected / Apply All
- [x] Validation and UndoableCommand execution

### Milestone 2 (Future)

- [ ] Mock provider for offline testing
- [ ] Gemini provider (different request format)
- [ ] Asset catalog population (models, shaders from VFS)
- [ ] Preview/ghost placement rendering

### Milestone 3 (Future)

- [ ] WebSocket streaming
- [ ] Local model support
- [ ] Semantic asset tagging
- [ ] Multi-step agent workflows

## Usage

1. **Tools → AI Assistant** to open the dock
2. Set `OPENAI_API_KEY` (or env var name in "API key env var")
3. Load a map, select an area or entity
4. Enter a prompt, e.g. "Place 3 props near this wall"
5. Click **Send Request**
6. Review the placement plan list
7. **Apply Selected** or **Apply All** to execute

## Example Request/Response JSON

### EditorContext (outgoing)

```json
{
  "mapPath": "maps/mylevel.map",
  "mapName": "mylevel.map",
  "gridSize": 8,
  "camera": {
    "origin": [256, 128, 64],
    "angles": [0, 45, 0],
    "viewVector": [0.7, 0.7, 0]
  },
  "cursorOrFocal": { "position": [256, 128, 64] },
  "selectionBounds": {
    "origin": [200, 100, 0],
    "extents": [64, 64, 96]
  },
  "selectedItems": [
    {
      "classname": "func_static",
      "modelPath": "models/props/table.ase",
      "position": [220, 120, 0],
      "angles": [0, 90, 0],
      "scale": [1, 1, 1]
    }
  ],
  "nearbyEntities": [...],
  "allowedEntities": ["misc_model", "light", "info_player_start", ...]
}
```

### PlacementPlan (incoming)

```json
{
  "placement_plan": [
    {
      "action": "place_model",
      "classname": "misc_model",
      "modelPath": "models/props/chair.ase",
      "position": [240, 140, 0],
      "angles": [0, 45, 0],
      "scale": [1, 1, 1],
      "confidence": 0.9,
      "reason": "Chair near table"
    }
  ],
  "summary": "Added chair and lamp near table"
}
```

## Example Prompts

- **"Dress this room"** – Suggest props for the selected area
- **"Place three props near this wall"** – Constrained placement
- **"Analyze dead space in this courtyard"** – Analysis (returns text; placement optional)
- **"Place cover objects or lights with justification"** – Gameplay-aware suggestions

## Security

- API keys are read from environment variables only (never hardcoded)
- Key env var name is configurable
- Secrets are redacted in logs (TODO: add redaction in log output)

## Build

Requires Qt5Network. Add to pkg-config path if needed.

```bash
make radiant
```

## Testing

1. Set `export OPENAI_API_KEY=sk-...`
2. Run Radiant, load a map
3. Tools → AI Assistant
4. Send a simple prompt; verify response appears
5. Apply one action; verify entity is created and Undo works
