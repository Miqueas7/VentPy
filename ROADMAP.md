# Roadmap · VentPy

VentPy nació con la norma peruana (**DS 024-2016-EM**). El objetivo es que sirva en toda
Latinoamérica y cubra el cálculo de ventilación completo.

> **¿Quieres contribuir?** Lee [`CONTRIBUTING.md`](CONTRIBUTING.md). Las tareas marcadas
> **`buena primera tarea`** están pensadas para quien nunca ha contribuido a open source:
> son de conocimiento del dominio, no de programación avanzada.

---

## 1. Multi-norma — la prioridad

Hoy las constantes normativas están fijas a Perú. El diseño objetivo es un
`RegulatoryConfig` por país, seleccionable, con cada valor citando su artículo.

| País | Norma | Estado | Etiqueta |
|---|---|---|---|
| 🇵🇪 Perú | DS 024-2016-EM | ✅ implementado | — |
| 🇨🇱 Chile | DS 132 · Reglamento de Seguridad Minera | ⬜ **abierto** | `buena primera tarea` |
| 🇨🇴 Colombia | Decreto 1886 de 2015 | ⬜ **abierto** | `buena primera tarea` |
| 🇲🇽 México | NOM-023-STPS-2012 | ⬜ **abierto** | `buena primera tarea` |

**Qué implica agregar una norma** (esto es todo — no hay que tocar C++ avanzado):

1. Ubicar en el reglamento los valores de: caudal mínimo por persona, factor por equipo
   diésel (m³/min por HP o kW), tiempo/caudal de dilución post-voladura, velocidades
   mínimas y máximas permitidas, y factor de fugas.
2. Añadirlos como un preset de configuración, **citando el artículo de cada valor**.
3. Escribir un test con un caso conocido de ese país.

Si no sabes programar en C++, **abre un issue con los valores y la cita del artículo** — eso
ya es la mitad del trabajo y alguien más lo integra.

---

## 2. Módulos por madurar

| Módulo | Estado |
|---|---|
| Personal, diésel, voladura, atmósfera, fugas, gobernador | ✅ maduro |
| **Polvo** (`dust`) | 🟡 modelo de resultado existe, implementación por madurar |
| **Térmico** (`thermal`) | 🟡 igual que polvo |

---

## 3. Buenas primeras tareas (no normativas)

- ⬜ **Ejemplos resueltos** en la documentación: un caso de mina superficial y uno subterránea,
  con sus números, ejecutables como test. `buena primera tarea`
- ⬜ **Traducir el README** a inglés técnico revisado / portugués. `buena primera tarea`
- ⬜ **Interfaz de línea de comandos** (`ventpy calcular --personal 20 --diesel 300`).
- ⬜ **Gráficos**: mejorar `visualization` con un diagrama de red de ventilación.
- ⬜ **Reportar un número que no calce** con tu experiencia de campo (¡con el valor esperado!).
  `buena primera tarea`

---

## 4. Cómo se decide

Este proyecto se usa para decisiones de **seguridad en minas**. Por eso:

- Ningún número entra sin su **referencia normativa**.
- La documentación es **conservadora**: si un módulo no está maduro, se dice.
- Se prefiere **rechazar un aporte** antes que publicar un cálculo que alguien use mal.
