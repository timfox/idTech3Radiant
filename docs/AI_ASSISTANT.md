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

### Milestone 2 (Implemented)

- [x] Mock provider for offline testing
- [x] Gemini provider (generateContent API, x-goog-api-key auth)
- [x] Asset catalog population (shaders from GlobalShaderSystem, models from VFS models/)
- [x] Grid size in context (GetGridSize)
- [x] Snap placement positions to grid on execution
- [ ] Preview/ghost placement rendering

### Image Generation (Implemented)

- [x] **iris.c** – Local FLUX.2 image generation via subprocess (https://github.com/antirez/iris.c)
- [x] **DALL-E** – OpenAI images API (requires `OPENAI_API_KEY`)
- [x] **Mock** – Placeholder for testing
- [x] Save to game textures with auto-generated Q3 shader
- [x] Apply generated texture to selected faces

### Milestone 3 (Future)

- [ ] WebSocket streaming
- [ ] Local model support (placement)
- [ ] Semantic asset tagging
- [ ] Multi-step agent workflows

## Usage

1. **Preferences → Settings → AI Assistant**: Enable the feature, set the active agent, and optionally configure Image Generation (iris path, model)
2. **Tools → AI Assistant** to open the dock
3. Select an agent from the dropdown (or add one with **+**)
4. For API keys: either set the env var (e.g. `OPENAI_API_KEY` for OpenAI, `GEMINI_API_KEY` for Gemini) or uncheck "Use environment variable" and enter the key directly (stored in preferences)
5. Load a map, select an area or entity
6. Enter a prompt, e.g. "Place 3 props near this wall"
7. Click **Send Request**
8. Review the placement plan list
9. **Apply Selected** or **Apply All** to execute

### Image Generation

1. Open the **Image Generation** tab in the AI Assistant dock
2. Enter a prompt (e.g. "A stone wall texture, weathered and mossy")
3. Choose provider: **iris** (local), **DALL-E** (cloud), or **Mock**
4. For **iris**: Set the path to the iris executable (build from https://github.com/antirez/iris.c) and model dir (e.g. `flux-klein-4b`)
5. Click **Generate**
6. **Save to textures** – Saves to `textures/ai_gen/` and creates a Q3 shader in `scripts/ai_gen.shader`
7. **Apply to selection** – Applies the generated texture to selected brush faces

## Preferences

- **Enable AI Assistant** – Master switch (Settings → AI Assistant)
- **Active agent** – Default agent name
- **Agents** – Stored as JSON; add/remove via dock **+** / **-** buttons. Each agent has: name, provider (OpenAI/Gemini/Mock), endpoint, model, API key source (env var or direct)
- **Image: iris path** – Path to iris executable (from iris.c)
- **Image: iris model** – Model directory (e.g. flux-klein-4b)

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

## Providers

- **OpenAI** – Chat completions API, Bearer token auth
- **Gemini** – Google AI generateContent API, x-goog-api-key header. Default endpoint: `generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent`
- **Mock** – Returns a static light placement for offline testing

## Testing

1. Set `export OPENAI_API_KEY=sk-...` or `export GEMINI_API_KEY=...`
2. Run Radiant, load a map
3. Tools → AI Assistant
4. Select agent (OpenAI, Gemini, or Mock)
5. Send a simple prompt; verify response appears
6. Apply one action; verify entity is created, snapped to grid, and Undo works
