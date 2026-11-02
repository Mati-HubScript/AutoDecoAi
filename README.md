# Decorador IA (Geode · Geometry Dash)

Mod para el editor de Geometry Dash. Seleccionas una parte del nivel, abres el menú con el botón **IA**, escribes una referencia (ej. *"decóralo tipo demoníaco"*), pulsas **Generar** y la IA coloca objetos del editor alrededor de tu selección. Menú minimalista con botón **Cerrar**.

## Compilar
1. Cambia `id` y `developer` en `mod.json`.
2. Sube el repo a GitHub: el workflow `.github/workflows/build.yml` compila para Windows, macOS y Android y deja el `.geode` en *Actions → Artifacts → Build Output*.
3. O en local: instala el [Geode SDK y la CLI](https://docs.geode-sdk.org/getting-started/) y ejecuta `geode build`.

## Proveedores de IA (ajustes del mod)
| Proveedor | Costo | Cómo configurarlo |
|---|---|---|
| **Gemini** (por defecto) | Nivel gratuito con límites | Saca una key en https://aistudio.google.com/apikey y pégala en *Gemini: API key* |
| **Ollama** | Gratis, local (solo PC) | Instala Ollama, ejecuta `ollama pull llama3.1`, déjalo abierto y elige "Ollama" |
| **Anthropic** | De pago | Key de console.anthropic.com en *Anthropic: API key* |

Las keys se guardan en texto plano: no las subas al repo.

## Otros ajustes
- **IDs permitidos**: la IA solo puede colocar estos objetos. Usa IDs de decoración **sin hitbox** para no alterar la jugabilidad (los bloques sólidos sí tienen hitbox). Amplía la lista para más variedad.
- **Máx. objetos nuevos**.

## Notas
- Los objetos creados entran en el historial de deshacer (Ctrl+Z).
- Los modelos pequeños de Ollama a veces devuelven JSON incorrecto; si falla, prueba un modelo más grande.
