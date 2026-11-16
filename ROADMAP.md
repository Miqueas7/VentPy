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
| 🇨🇱 Chile, pequeña minería | DS 132, Título XV (faenas de hasta 5.000 t/mes) | ✅ factor diésel (0.3.0): `chile_small_mining()` | |
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

### Pendientes normativos

Valores que las normas fijan y la librería todavía no calcula ni verifica. Cada
uno tiene su artículo identificado en el texto vigente; donde hoy hay una cita,
solo se nombra, sin lógica detrás.

| Norma | Pendiente |
|---|---|
| Chile, DS 132 | Pequeña minería (Título XV): aforo trimestral con control anual (Art. 628). El factor de 3 m³/min por HP (Art. 627) ya está en `chile_small_mining()` |
| Perú, DS 024 | Minas de carbón (Arts. 258 y 259): 4,5 m³/min por hombre hasta 1.500 msnm con la escala del Art. 247, y el doble en zona «gaseada» (CH4 > 0,5 %) |
| Colombia, Decreto 1886 | Minas de carbón: mínimas por tipo de vía (Art. 56 nums. 1 a 3), categorías y medición de metano (Arts. 53, 58, 60 y 162). Opción de 4 m³/min por HP con CO en el escape ≤ 0,08 % (Art. 54 par. 2 num. 2) |
| México | Minas de carbón (NOM-032-STPS-2008) |
| Perú, DS 024 | Balance de ingresos y salidas de aire de la mina con diferencia ≤ 10 % (Art. 252 lit. c). En Chile, pérdidas ≤ 15 % en el control general (Art. 139) |
| Perú, Chile, México | Límites de gases en el escape diésel: Perú CO < 500 ppm y NO2 ≤ 100 ppm (Art. 254); Chile CO 2.000 y NOx 1.000 ppm en el escape, y CO 40, NOx 20 y aldehído fórmico 1,6 ppm en el ambiente para detener equipos (Art. 135); México CO 0,25 %, NO2 0,10 %, SO2 0,10 % y aldehídos 0,001 % (8.11.6 e) |
| Perú, Chile, México | Ventilación auxiliar: obligatoria en labores de una vía con más de 60 m de avance y mangas a ≤ 15 m del frente (Perú, Art. 246 lit. f); extremo del ducto a ≤ 30 m del frente (Chile, Art. 141; México, 8.4.4 c) y tubería de emergencia a < 5 m (8.4.4 e) |
| Perú, DS 024 | Panel de control en ventiladores principales de 2.831 m³/min o más (Art. 251) |
| Perú, México | Polvo inhalable: Perú 10 mg/m³ y trípoli 0,1 mg/m³ (Anexo 15); México partículas inhalables 10 mg/m³ (NOM-010, fila 609). La sílice respirable ya fija el objetivo de polvo en Perú, Chile y México |
| Todas | Verificación explícita del reingreso tras la voladura: Perú Art. 240; Colombia 15 min (Art. 157) y 30 min tras voladura fallida (Art. 158); México 30 min y sin humo (8.6.9 g) |
| Colombia, México, Chile | Carga térmica por temperatura efectiva (Colombia, Arts. 217 y 218) o índice TGBH (México, NOM-015-STPS-2001; Chile, DS 594 Arts. 96 a 98) |
| Chile, Colombia, México | Corrección de límites por jornada: Chile Fj (DS 594, Arts. 62 y 64), Colombia Brief y Scala (Art. 39 par. 2), México (NOM-010, 10.4.1). Chile: Fa del Art. 63 sobre los demás límites en mg/m³ y excursiones del Art. 60 |
| Perú, DS 024 | Piso térmico sobre 29 °C: el Art. 252 lit. d fija 30 m/min «de 24 °C hasta 29 °C como máximo» y no dice qué pasa encima; hoy el piso deja de exigirse sobre 29 °C |
| Todas | Advertencias de velocidad de `red.hpp` y `atkinson.hpp`: hoy citan siempre los 20 y 250 m/min del DS 024, Art. 248, también con otra norma; deberían seguir a la norma configurada |

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
