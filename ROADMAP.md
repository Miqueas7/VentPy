# Roadmap · VentPy

VentPy nació con la norma peruana (**DS 024-2016-EM**), desde la 0.2.0 incluye la chilena
(**DS 132**) y desde la 0.3.0 la colombiana y la mexicana. El objetivo es que sirva en
toda Latinoamérica y cubra el cálculo de ventilación completo.

> **¿Quieres contribuir?** Lee [`CONTRIBUTING.md`](CONTRIBUTING.md). Las tareas marcadas
> **`buena primera tarea`** están pensadas para quien nunca ha contribuido a open source:
> son de conocimiento del dominio, no de programación avanzada.

---

## 1. Multi-norma: la prioridad

Cada país es un preajuste de `RegulatoryConfig` (`peru()`, `chile()`, `colombia()`,
`mexico()`), seleccionable con `for_standard(...)` o con `--norma` en la línea de comandos.
Cada valor cita su artículo y la cita impresa en el resultado sigue a la norma configurada.

| País | Norma | Estado | Etiqueta |
|---|---|---|---|
| 🇵🇪 Perú | DS 024-2016-EM / DS 023-2017-EM | ✅ implementado | |
| 🇨🇱 Chile | DS 132 · Reglamento de Seguridad Minera (gases: DS 594) | ✅ implementado (0.2.0) | |
| 🇨🇴 Colombia | Decreto 1886 de 2015 (mod. Decreto 944 de 2022) | ✅ implementado (0.3.0) | |
| 🇲🇽 México | NOM-023-STPS-2012 (gases: NOM-010-STPS-2014) | ✅ implementado (0.3.0) | |
| 🇲🇽 México, minas de carbón | NOM-032-STPS-2008 (con proyecto de reemplazo en consulta) | ⬜ **abierto** | |
| Otros países | Bolivia, Ecuador, Argentina, Brasil… | ⬜ **abierto** | `buena primera tarea` |

Ningún valor entra al código antes de que su tabla valor → artículo → cita esté validada
contra el texto vigente. Si conoces la norma de tu país, armar o revisar esa tabla es la
ayuda que más sirve.

**Qué implica agregar una norma** (los preajustes existentes son el modelo a copiar):

1. Ubicar en el reglamento los valores de: caudal mínimo por persona, factor por equipo
   diésel (m³/min por HP o kW), tiempo o caudal de dilución tras la voladura, velocidades
   mínimas y máximas permitidas, factor de fugas y límites de gases.
2. Añadirlos como un preajuste en `normativa.hpp` y su tabla de gases en `limites_gases.hpp`,
   **citando el artículo de cada valor**. Si la norma no regula un concepto, la cita lo dice;
   no se toma prestado el artículo de otro país.
3. Escribir un test con un caso conocido de ese país.

Si no sabes programar en C++, **abre un issue con los valores y la cita del artículo**: eso
ya es la mitad del trabajo y alguien más lo integra.

---

## 2. Módulos

| Módulo | Estado |
|---|---|
| Personal, diésel, voladura, atmósfera, fugas, gobernador | ✅ maduro |
| Polvo y térmico | ✅ implementados (0.2.0). Fuera de Perú la cita declara qué fija la norma y qué queda como criterio de ingeniería |
| Calor por temperatura efectiva (Colombia) o índice TGBH (México) | ⬜ abierto: hoy solo se cita |
| Cobertura y déficit (caudal medido frente al requerido) | ✅ implementado (0.2.0) |
| Resistencia de Atkinson, ducto, red por Hardy Cross, ventilador | ✅ implementado (0.2.0) |
| Límites de gases por norma | ✅ implementado (0.2.0) |

---

## 3. Buenas primeras tareas (no normativas)

- ✅ **Ejemplos resueltos**: cinco casos en [`examples/`](examples/), cada uno verifica sus
  propios números. Falta todavía un caso con datos de campo. `buena primera tarea`
- ✅ **Interfaz de línea de comandos**: `ventpy demanda`, `ventpy lmp`, `ventpy cobertura` y más.
- ⬜ **Traducir el README** al portugués (ya está en español e inglés). `buena primera tarea`
- ⬜ **Gráficos**: mejorar `visualization` con un diagrama de red de ventilación.
- ⬜ **Reportar un número que no calce** con tu experiencia de campo (¡con el valor esperado!).
  `buena primera tarea`

---

## 4. Cómo se decide

Este proyecto se usa para decisiones de **seguridad en minas**. Por eso:

- Ningún número entra sin su **referencia normativa**.
- La documentación es **conservadora**: si un módulo no está maduro, se dice.
- Se prefiere **rechazar un aporte** antes que publicar un cálculo que alguien use mal.
