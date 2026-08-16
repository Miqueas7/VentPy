# Contributing to VentPy · Cómo contribuir a VentPy

VentPy is an open-source library for **mine ventilation calculations**. It started with the
Peruvian standard (DS 024-2016-EM) and is growing toward **multi-standard support across Latin
America**: Chile, Colombia and Mexico.

> 🇪🇸 **Este documento está en inglés y español.** Baja para la versión en español.
> **No necesitas ser programador experto.** Si eres ingeniero de minas y conoces la norma de tu
> país, ya tienes lo más difícil: el conocimiento del dominio.

---

## English

### Ways to contribute

You do **not** need to write C++ to help. In order of how much we need it:

1. **Add a national standard.** You know your country's ventilation regulation? That's the
   contribution we need most. See `ROADMAP.md` for the open standards.
2. **Worked examples.** A real (anonymised) ventilation case with its numbers, as a test.
3. **Documentation.** Fix an unclear paragraph, translate, add a diagram.
4. **Report a wrong number.** If a result doesn't match your field experience, open an issue.
   A bug report with the expected value is a genuine contribution.
5. **Code.** Python API, plots, CLI, performance.

### First contribution, step by step

```bash
# 1. Fork the repo on GitHub, then:
git clone https://github.com/YOUR-USER/VentPy.git
cd VentPy

# 2. Install in editable mode with test extras
pip install -e ".[test,viz]"

# 3. Run the tests (should be green before you change anything)
pytest tests/python -q

# 4. Create a branch, make your change, run tests again
git checkout -b add-chile-standard
pytest tests/python -q

# 5. Commit and open a Pull Request
```

### Ground rules

- **Cite the regulation.** Any constant must reference its article: `DS 024-2016-EM, Art. 247`.
  Numbers without a source will not be merged — this library is used for **safety** decisions.
- **Add a test** for every new behaviour. If you're unsure how, open the PR anyway and ask.
- **Be conservative in the docs.** Never claim more precision than the method actually has.
- Small PRs are better than big ones. One standard, one PR.

### Questions?

Open an issue with the `question` label. There are no stupid questions — if something wasn't
clear to you, it wasn't clear enough.

---

## Español

### Formas de contribuir

**No necesitas escribir C++.** En orden de lo que más falta:

1. **Agregar la norma de tu país.** ¿Conoces el reglamento de ventilación de Chile, Colombia o
   México? Esa es la contribución que más necesitamos. Mira `ROADMAP.md`.
2. **Casos resueltos.** Un caso real de ventilación (anonimizado) con sus números, como test.
3. **Documentación.** Corregir un párrafo confuso, traducir, agregar un diagrama.
4. **Reportar un número equivocado.** Si un resultado no calza con tu experiencia en campo,
   abre un issue. Un reporte con el valor esperado ya es una contribución real.
5. **Código.** API de Python, gráficos, línea de comandos, rendimiento.

### Tu primera contribución, paso a paso

```bash
# 1. Haz fork del repo en GitHub, luego:
git clone https://github.com/TU-USUARIO/VentPy.git
cd VentPy

# 2. Instala en modo editable con extras de test
pip install -e ".[test,viz]"

# 3. Corre los tests (deben pasar antes de que toques nada)
pytest tests/python -q

# 4. Crea una rama, haz tu cambio, corre los tests otra vez
git checkout -b agregar-norma-chile
pytest tests/python -q

# 5. Commitea y abre un Pull Request
```

### Reglas de la casa

- **Cita la norma.** Toda constante debe referenciar su artículo: `DS 024-2016-EM, Art. 247`.
  Un número sin fuente no se mergea — esta librería se usa para decisiones de **seguridad**.
- **Agrega un test** por cada comportamiento nuevo. Si no sabes cómo, abre el PR igual y pregunta.
- **Sé conservador en la documentación.** Nunca afirmes más precisión de la que el método tiene.
- Mejor PRs chicos que grandes. Una norma, un PR.

### ¿Dudas?

Abre un issue con la etiqueta `question`. No hay preguntas tontas: si algo no te quedó claro,
es que no estaba lo bastante claro.
