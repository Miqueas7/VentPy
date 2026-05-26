"""
VentPy Visualization Module.

Provides plotting and reporting capabilities for mine ventilation calculations.

Features:
- Bar charts comparing Q_Per, Q_Eq, Q_Exp, Q_Dust, Q_Thermal
- Pie charts showing distribution of ventilation demand
- Altitude correction curves
- Summary dashboards
- Report generation (HTML/PDF-ready)

Requires: matplotlib, numpy (optional: plotly for interactive charts)

Reference: DS 024-2016-EM (Peru)

Copyright 2026 VentPy Project
"""

from __future__ import annotations

import io
import warnings
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple, Union

# Optional imports with graceful fallback
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    from matplotlib.figure import Figure
    from matplotlib.axes import Axes
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    plt = None
    Figure = None
    Axes = None

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False
    np = None


# =============================================================================
# Color Schemes and Styling
# =============================================================================

@dataclass
class VentPyStyle:
    """Style configuration for VentPy charts."""
    
    # Primary colors for flow components
    colors: Dict[str, str] = field(default_factory=lambda: {
        'personnel': '#2E86AB',      # Blue
        'diesel': '#A23B72',         # Magenta
        'blasting': '#F18F01',       # Orange
        'dust': '#C73E1D',           # Red
        'thermal': '#3B1F2B',        # Dark
        'leakage': '#95A3A4',        # Gray
        'total': '#1B4332',          # Dark Green
        'governing': '#2D6A4F',      # Green
    })
    
    # Color palette for altitude zones
    altitude_colors: List[str] = field(default_factory=lambda: [
        '#4CAF50',  # 0-2000m (green - comfortable)
        '#FFC107',  # 2000-3000m (yellow - moderate)
        '#FF9800',  # 3000-4000m (orange - elevated)
        '#F44336',  # 4000-4500m (red - high)
        '#9C27B0',  # >4500m (purple - extreme)
    ])
    
    # Figure styling
    figure_facecolor: str = '#FAFAFA'
    axes_facecolor: str = '#FFFFFF'
    grid_color: str = '#E0E0E0'
    text_color: str = '#212121'
    
    # Font sizes
    title_fontsize: int = 14
    label_fontsize: int = 11
    tick_fontsize: int = 10
    legend_fontsize: int = 10
    
    # Bar chart
    bar_width: float = 0.6
    bar_edgecolor: str = '#333333'
    bar_linewidth: float = 1.0


# Default style
DEFAULT_STYLE = VentPyStyle()


# =============================================================================
# Data Extraction Helpers
# =============================================================================

def extract_flow_data(result) -> Dict[str, float]:
    """
    Extract flow values from a VentilationDemandResult.
    
    Args:
        result: VentilationDemandResult from VentPy calculation
        
    Returns:
        Dictionary with flow component names and values in m3/min
    """
    data = {}
    
    # Try to get values (handle both object and dict)
    if hasattr(result, 'q_personnel_m3min'):
        data['personnel'] = getattr(result, 'q_personnel_m3min', 0.0) or 0.0
        data['diesel'] = getattr(result, 'q_diesel_m3min', 0.0) or 0.0
        data['blasting'] = getattr(result, 'q_blasting_m3min', 0.0) or 0.0
        data['dust'] = getattr(result, 'q_dust_m3min', 0.0) or 0.0
        data['thermal'] = getattr(result, 'q_thermal_m3min', 0.0) or 0.0
        data['leakage'] = getattr(result, 'q_leakage_m3min', 0.0) or 0.0
        data['governing'] = getattr(result, 'q_governing_m3min', 0.0) or 0.0
        data['total'] = getattr(result, 'q_total_m3min', 0.0) or 0.0
    elif isinstance(result, dict):
        data = {
            'personnel': result.get('q_personnel_m3min', 0.0) or 0.0,
            'diesel': result.get('q_diesel_m3min', 0.0) or 0.0,
            'blasting': result.get('q_blasting_m3min', 0.0) or 0.0,
            'dust': result.get('q_dust_m3min', 0.0) or 0.0,
            'thermal': result.get('q_thermal_m3min', 0.0) or 0.0,
            'leakage': result.get('q_leakage_m3min', 0.0) or 0.0,
            'governing': result.get('q_governing_m3min', 0.0) or 0.0,
            'total': result.get('q_total_m3min', 0.0) or 0.0,
        }
    else:
        raise ValueError("Result must be VentilationDemandResult or dict")
    
    return data


def get_flow_labels() -> Dict[str, str]:
    """Get display labels for flow components."""
    return {
        'personnel': 'Personal (Q_Per)',
        'diesel': 'Diesel (Q_Eq)',
        'blasting': 'Voladura (Q_Exp)',
        'dust': 'Polvo (Q_Dust)',
        'thermal': 'Termico (Q_Th)',
        'leakage': 'Fugas (Q_Fug)',
        'governing': 'Gobernante',
        'total': 'Total',
    }


# =============================================================================
# Bar Chart Functions
# =============================================================================

def plot_flow_comparison(
    result,
    title: str = "Comparacion de Caudales de Ventilacion",
    unit: str = "m3/min",
    show_total: bool = True,
    show_leakage: bool = True,
    style: VentPyStyle = DEFAULT_STYLE,
    figsize: Tuple[float, float] = (10, 6),
    ax: Optional[Any] = None,
) -> Tuple[Any, Any]:
    """
    Create a bar chart comparing ventilation flow components.
    
    Args:
        result: VentilationDemandResult or dict with flow values
        title: Chart title
        unit: Unit label (m3/min, CFM, etc.)
        show_total: Whether to show total flow bar
        show_leakage: Whether to show leakage bar
        style: VentPyStyle configuration
        figsize: Figure size (width, height)
        ax: Existing axes to plot on (optional)
        
    Returns:
        Tuple of (figure, axes)
    """
    _check_matplotlib()
    
    data = extract_flow_data(result)
    labels_map = get_flow_labels()
    
    # Select which components to show
    components = ['personnel', 'diesel', 'blasting', 'dust', 'thermal']
    if show_leakage:
        components.append('leakage')
    if show_total:
        components.append('total')
    
    # Filter non-zero values
    values = [data.get(c, 0.0) for c in components]
    labels = [labels_map.get(c, c) for c in components]
    colors = [style.colors.get(c, '#888888') for c in components]
    
    # Create figure if needed
    if ax is None:
        fig, ax = plt.subplots(figsize=figsize, facecolor=style.figure_facecolor)
    else:
        fig = ax.get_figure()
    
    ax.set_facecolor(style.axes_facecolor)
    
    # Create bars
    x_pos = range(len(components))
    bars = ax.bar(
        x_pos, values,
        width=style.bar_width,
        color=colors,
        edgecolor=style.bar_edgecolor,
        linewidth=style.bar_linewidth,
    )
    
    # Add value labels on bars
    for bar, val in zip(bars, values):
        if val > 0:
            height = bar.get_height()
            ax.annotate(
                f'{val:.1f}',
                xy=(bar.get_x() + bar.get_width() / 2, height),
                xytext=(0, 3),
                textcoords="offset points",
                ha='center', va='bottom',
                fontsize=style.tick_fontsize,
                color=style.text_color,
            )
    
    # Formatting
    ax.set_xticks(x_pos)
    ax.set_xticklabels(labels, rotation=30, ha='right', fontsize=style.tick_fontsize)
    ax.set_ylabel(f'Caudal [{unit}]', fontsize=style.label_fontsize)
    ax.set_title(title, fontsize=style.title_fontsize, fontweight='bold')
    ax.grid(axis='y', linestyle='--', alpha=0.7, color=style.grid_color)
    ax.set_axisbelow(True)
    
    # Set y-axis to start at 0
    ax.set_ylim(bottom=0)
    
    plt.tight_layout()
    return fig, ax


def plot_flow_breakdown(
    result,
    title: str = "Distribucion de Demanda de Ventilacion",
    style: VentPyStyle = DEFAULT_STYLE,
    figsize: Tuple[float, float] = (8, 8),
    ax: Optional[Any] = None,
) -> Tuple[Any, Any]:
    """
    Create a pie chart showing the distribution of ventilation demand.
    
    Args:
        result: VentilationDemandResult or dict
        title: Chart title
        style: VentPyStyle configuration
        figsize: Figure size
        ax: Existing axes (optional)
        
    Returns:
        Tuple of (figure, axes)
    """
    _check_matplotlib()
    
    data = extract_flow_data(result)
    labels_map = get_flow_labels()
    
    # Only include main demand components (not leakage/total)
    components = ['personnel', 'diesel', 'blasting', 'dust', 'thermal']
    values = [data.get(c, 0.0) for c in components]
    
    # Filter out zero values
    non_zero = [(c, v) for c, v in zip(components, values) if v > 0]
    if not non_zero:
        warnings.warn("No positive flow values to display")
        fig, ax = plt.subplots(figsize=figsize)
        ax.text(0.5, 0.5, 'Sin datos', ha='center', va='center', fontsize=14)
        return fig, ax
    
    components, values = zip(*non_zero)
    labels = [labels_map.get(c, c) for c in components]
    colors = [style.colors.get(c, '#888888') for c in components]
    
    # Create figure if needed
    if ax is None:
        fig, ax = plt.subplots(figsize=figsize, facecolor=style.figure_facecolor)
    else:
        fig = ax.get_figure()
    
    # Create pie chart
    wedges, texts, autotexts = ax.pie(
        values,
        labels=labels,
        colors=colors,
        autopct='%1.1f%%',
        startangle=90,
        explode=[0.02] * len(values),
        shadow=False,
        wedgeprops={'edgecolor': style.bar_edgecolor, 'linewidth': 1},
    )
    
    # Style the percentage labels
    for autotext in autotexts:
        autotext.set_fontsize(style.tick_fontsize)
        autotext.set_fontweight('bold')
    
    ax.set_title(title, fontsize=style.title_fontsize, fontweight='bold')
    
    plt.tight_layout()
    return fig, ax


# =============================================================================
# Altitude Correction Charts
# =============================================================================

def plot_altitude_corrections(
    altitude_range: Tuple[float, float] = (0, 5000),
    num_points: int = 50,
    title: str = "Factores de Correccion por Altitud",
    style: VentPyStyle = DEFAULT_STYLE,
    figsize: Tuple[float, float] = (12, 8),
) -> Tuple[Any, Any]:
    """
    Plot altitude correction factors for ventilation calculations.
    
    Shows:
    - Atmospheric pressure ratio
    - Air density ratio
    - Volume correction factor
    - Diesel de-rating factor
    
    Args:
        altitude_range: (min, max) altitude in masl
        num_points: Number of data points
        title: Chart title
        style: VentPyStyle configuration
        figsize: Figure size
        
    Returns:
        Tuple of (figure, axes)
    """
    _check_matplotlib()
    _check_numpy()
    
    # Generate altitude points
    altitudes = np.linspace(altitude_range[0], altitude_range[1], num_points)
    
    # Calculate corrections (ISA model formulas)
    P0 = 101.325  # kPa at sea level
    T0 = 288.15   # K at sea level
    L = 0.0065    # K/m lapse rate
    g = 9.81
    M = 0.0289644
    R = 8.31447
    exp = (g * M) / (R * L)
    
    # Pressure ratio
    pressure_ratio = np.power(1.0 - (L * altitudes / T0), exp)
    
    # Density ratio (at standard temperature)
    density_ratio = pressure_ratio  # Simplified (isothermal approx for display)
    
    # Volume correction factor
    volume_factor = 1.0 / density_ratio
    
    # Diesel de-rating (turbo: 1.5% per 300m above 1000m)
    diesel_derate = np.ones_like(altitudes)
    above_1000 = altitudes > 1000
    diesel_derate[above_1000] = 1.0 - 0.015 * ((altitudes[above_1000] - 1000) / 300)
    diesel_derate = np.maximum(diesel_derate, 0.5)  # Cap at 50% loss
    
    # O2 partial pressure ratio
    o2_ratio = pressure_ratio
    
    # Create figure with subplots
    fig, axes = plt.subplots(2, 2, figsize=figsize, facecolor=style.figure_facecolor)
    fig.suptitle(title, fontsize=style.title_fontsize + 2, fontweight='bold')
    
    # Define altitude zones for background
    zone_limits = [0, 2000, 3000, 4000, 4500, altitude_range[1]]
    zone_labels = ['Baja', 'Moderada', 'Elevada', 'Alta', 'Extrema']
    
    def add_altitude_zones(ax, y_range):
        """Add colored altitude zone backgrounds."""
        for i, (start, end) in enumerate(zip(zone_limits[:-1], zone_limits[1:])):
            if end <= altitude_range[0] or start >= altitude_range[1]:
                continue
            start = max(start, altitude_range[0])
            end = min(end, altitude_range[1])
            ax.axvspan(start, end, alpha=0.15, color=style.altitude_colors[i])
    
    # Plot 1: Pressure and Density Ratios
    ax1 = axes[0, 0]
    add_altitude_zones(ax1, (0.4, 1.0))
    ax1.plot(altitudes, pressure_ratio, 'b-', linewidth=2, label='Presion P/P0')
    ax1.plot(altitudes, density_ratio, 'g--', linewidth=2, label='Densidad rho/rho0')
    ax1.set_xlabel('Altitud [msnm]', fontsize=style.label_fontsize)
    ax1.set_ylabel('Ratio [-]', fontsize=style.label_fontsize)
    ax1.set_title('Presion y Densidad Relativas', fontsize=style.label_fontsize)
    ax1.legend(fontsize=style.legend_fontsize)
    ax1.grid(True, linestyle='--', alpha=0.5)
    ax1.set_xlim(altitude_range)
    ax1.set_ylim(0.4, 1.05)
    
    # Plot 2: Volume Correction Factor
    ax2 = axes[0, 1]
    add_altitude_zones(ax2, (1.0, 2.0))
    ax2.plot(altitudes, volume_factor, 'r-', linewidth=2)
    ax2.fill_between(altitudes, 1.0, volume_factor, alpha=0.3, color='red')
    ax2.set_xlabel('Altitud [msnm]', fontsize=style.label_fontsize)
    ax2.set_ylabel('Factor [-]', fontsize=style.label_fontsize)
    ax2.set_title('Factor de Correccion Volumetrica', fontsize=style.label_fontsize)
    ax2.grid(True, linestyle='--', alpha=0.5)
    ax2.set_xlim(altitude_range)
    ax2.axhline(y=1.0, color='k', linestyle=':', alpha=0.5)
    
    # Add annotation
    idx_4000 = np.argmin(np.abs(altitudes - 4000))
    ax2.annotate(
        f'{volume_factor[idx_4000]:.2f}x\na 4000m',
        xy=(4000, volume_factor[idx_4000]),
        xytext=(3000, volume_factor[idx_4000] + 0.2),
        fontsize=10,
        arrowprops=dict(arrowstyle='->', color='black'),
    )
    
    # Plot 3: Diesel De-rating
    ax3 = axes[1, 0]
    add_altitude_zones(ax3, (0.5, 1.0))
    ax3.plot(altitudes, diesel_derate * 100, 'purple', linewidth=2)
    ax3.fill_between(altitudes, 100, diesel_derate * 100, alpha=0.3, color='purple')
    ax3.set_xlabel('Altitud [msnm]', fontsize=style.label_fontsize)
    ax3.set_ylabel('Potencia Disponible [%]', fontsize=style.label_fontsize)
    ax3.set_title('De-rating Motor Diesel (Turbo)', fontsize=style.label_fontsize)
    ax3.grid(True, linestyle='--', alpha=0.5)
    ax3.set_xlim(altitude_range)
    ax3.set_ylim(50, 105)
    ax3.axhline(y=100, color='k', linestyle=':', alpha=0.5)
    ax3.axvline(x=1000, color='gray', linestyle='--', alpha=0.5)
    ax3.text(1050, 55, 'Umbral 1000m', fontsize=9, color='gray')
    
    # Plot 4: O2 Availability (with danger zone)
    ax4 = axes[1, 1]
    add_altitude_zones(ax4, (10, 22))
    o2_kpa = o2_ratio * 101.325 * 0.2095  # Partial pressure in kPa
    ax4.plot(altitudes, o2_kpa, 'teal', linewidth=2)
    ax4.axhline(y=13.3, color='red', linestyle='--', linewidth=1.5, 
                label='Limite fisiologico (~4000m)')
    ax4.set_xlabel('Altitud [msnm]', fontsize=style.label_fontsize)
    ax4.set_ylabel('Presion Parcial O2 [kPa]', fontsize=style.label_fontsize)
    ax4.set_title('Disponibilidad de Oxigeno', fontsize=style.label_fontsize)
    ax4.legend(fontsize=style.legend_fontsize)
    ax4.grid(True, linestyle='--', alpha=0.5)
    ax4.set_xlim(altitude_range)
    
    plt.tight_layout()
    return fig, axes


# =============================================================================
# Dashboard / Summary View
# =============================================================================

def create_dashboard(
    result,
    title: str = "Resumen de Ventilacion",
    style: VentPyStyle = DEFAULT_STYLE,
    figsize: Tuple[float, float] = (14, 10),
) -> Tuple[Any, List[Any]]:
    """
    Create a comprehensive dashboard with multiple charts.
    
    Includes:
    - Bar chart of flow components
    - Pie chart of distribution
    - Key metrics summary
    - Warnings display
    
    Args:
        result: VentilationDemandResult
        title: Dashboard title
        style: VentPyStyle configuration
        figsize: Figure size
        
    Returns:
        Tuple of (figure, list of axes)
    """
    _check_matplotlib()
    
    data = extract_flow_data(result)
    
    # Create figure with grid layout
    fig = plt.figure(figsize=figsize, facecolor=style.figure_facecolor)
    fig.suptitle(title, fontsize=style.title_fontsize + 4, fontweight='bold', y=0.98)
    
    # Create grid: 2 rows, 3 columns
    # Row 1: Bar chart (2 cols), Metrics (1 col)
    # Row 2: Pie chart (1 col), Info panel (2 cols)
    
    ax_bar = fig.add_subplot(2, 3, (1, 2))
    ax_metrics = fig.add_subplot(2, 3, 3)
    ax_pie = fig.add_subplot(2, 3, 4)
    ax_info = fig.add_subplot(2, 3, (5, 6))
    
    # 1. Bar chart
    plot_flow_comparison(result, title="", style=style, ax=ax_bar)
    ax_bar.set_title("Comparacion de Caudales", fontsize=style.label_fontsize, fontweight='bold')
    
    # 2. Pie chart
    plot_flow_breakdown(result, title="", style=style, ax=ax_pie)
    ax_pie.set_title("Distribucion (%)", fontsize=style.label_fontsize, fontweight='bold')
    
    # 3. Key Metrics Panel
    ax_metrics.set_facecolor(style.axes_facecolor)
    ax_metrics.axis('off')
    
    metrics_text = [
        ("CAUDAL TOTAL", f"{data['total']:.1f} m3/min"),
        ("", f"{data['total'] * 35.3147:.0f} CFM"),
        ("", ""),
        ("Gobernante", f"{data['governing']:.1f} m3/min"),
        ("Fugas", f"{data['leakage']:.1f} m3/min"),
    ]
    
    # Add governing factor if available
    if hasattr(result, 'governing_factor'):
        gf = getattr(result, 'governing_factor', 'N/A')
        metrics_text.append(("Factor", str(gf)))
    
    y_pos = 0.95
    for label, value in metrics_text:
        if label:
            ax_metrics.text(0.1, y_pos, label + ":", fontsize=11, fontweight='bold',
                          transform=ax_metrics.transAxes, va='top')
        if value:
            ax_metrics.text(0.9, y_pos, value, fontsize=11,
                          transform=ax_metrics.transAxes, va='top', ha='right',
                          color=style.colors['total'] if 'TOTAL' in label else style.text_color)
        y_pos -= 0.12
    
    ax_metrics.set_title("Metricas Clave", fontsize=style.label_fontsize, 
                        fontweight='bold', pad=10)
    
    # 4. Info Panel (warnings, notes)
    ax_info.set_facecolor('#FFF9C4')  # Light yellow for attention
    ax_info.axis('off')
    
    info_lines = []
    
    # Get warnings if available
    warnings_list = getattr(result, 'warnings', []) if hasattr(result, 'warnings') else []
    if warnings_list:
        info_lines.append("ADVERTENCIAS:")
        for w in warnings_list[:5]:  # Limit to 5
            info_lines.append(f"  ! {w}")
        info_lines.append("")
    
    # Get notes if available
    notes = getattr(result, 'notes', '') if hasattr(result, 'notes') else ''
    if notes:
        info_lines.append(f"Notas: {notes}")
    
    # Regulatory reference
    info_lines.append("")
    info_lines.append("Normativa: DS 024-2016-EM (Peru)")
    
    info_text = '\n'.join(info_lines) if info_lines else "Sin advertencias"
    ax_info.text(0.02, 0.95, info_text, fontsize=10, transform=ax_info.transAxes,
                va='top', family='monospace',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    ax_info.set_title("Informacion y Advertencias", fontsize=style.label_fontsize,
                     fontweight='bold', pad=10)
    
    plt.tight_layout()
    plt.subplots_adjust(top=0.93)
    
    return fig, [ax_bar, ax_metrics, ax_pie, ax_info]


# =============================================================================
# Report Generation
# =============================================================================

def generate_html_report(
    result,
    title: str = "Reporte de Ventilacion",
    include_charts: bool = True,
    style: VentPyStyle = DEFAULT_STYLE,
) -> str:
    """
    Generate an HTML report from ventilation calculation results.
    
    Args:
        result: VentilationDemandResult
        title: Report title
        include_charts: Whether to embed charts as base64 images
        style: VentPyStyle configuration
        
    Returns:
        HTML string
    """
    data = extract_flow_data(result)
    labels = get_flow_labels()
    
    # Start HTML
    html_parts = [
        "<!DOCTYPE html>",
        "<html lang='es'>",
        "<head>",
        "  <meta charset='UTF-8'>",
        f"  <title>{title}</title>",
        "  <style>",
        "    body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }",
        "    .container { max-width: 1000px; margin: auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }",
        "    h1 { color: #1B4332; border-bottom: 3px solid #2D6A4F; padding-bottom: 10px; }",
        "    h2 { color: #2D6A4F; margin-top: 30px; }",
        "    table { width: 100%; border-collapse: collapse; margin: 20px 0; }",
        "    th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }",
        "    th { background-color: #2D6A4F; color: white; }",
        "    tr:hover { background-color: #f5f5f5; }",
        "    .highlight { font-size: 1.5em; color: #1B4332; font-weight: bold; }",
        "    .warning { background: #FFF3CD; border-left: 4px solid #FFC107; padding: 10px; margin: 10px 0; }",
        "    .metric-box { display: inline-block; background: #E8F5E9; padding: 15px 25px; margin: 10px; border-radius: 8px; text-align: center; }",
        "    .metric-value { font-size: 2em; color: #1B4332; font-weight: bold; }",
        "    .metric-label { color: #666; font-size: 0.9em; }",
        "    .chart-container { text-align: center; margin: 20px 0; }",
        "    .chart-container img { max-width: 100%; border: 1px solid #ddd; border-radius: 4px; }",
        "    .footer { margin-top: 40px; padding-top: 20px; border-top: 1px solid #ddd; color: #666; font-size: 0.9em; }",
        "  </style>",
        "</head>",
        "<body>",
        "<div class='container'>",
        f"  <h1>{title}</h1>",
    ]
    
    # Key metrics section
    html_parts.extend([
        "  <h2>Resultados Principales</h2>",
        "  <div style='text-align: center;'>",
        f"    <div class='metric-box'><div class='metric-value'>{data['total']:.1f}</div><div class='metric-label'>Caudal Total (m3/min)</div></div>",
        f"    <div class='metric-box'><div class='metric-value'>{data['total'] * 35.3147:.0f}</div><div class='metric-label'>Caudal Total (CFM)</div></div>",
        f"    <div class='metric-box'><div class='metric-value'>{data['governing']:.1f}</div><div class='metric-label'>Caudal Gobernante (m3/min)</div></div>",
        "  </div>",
    ])
    
    # Detailed breakdown table
    html_parts.extend([
        "  <h2>Desglose de Caudales</h2>",
        "  <table>",
        "    <tr><th>Componente</th><th>Caudal (m3/min)</th><th>Caudal (CFM)</th><th>% del Gobernante</th></tr>",
    ])
    
    governing = data['governing'] if data['governing'] > 0 else 1.0
    for key in ['personnel', 'diesel', 'blasting', 'dust', 'thermal']:
        val = data.get(key, 0.0)
        if val > 0:
            pct = (val / governing) * 100
            html_parts.append(
                f"    <tr><td>{labels[key]}</td><td>{val:.2f}</td><td>{val * 35.3147:.0f}</td><td>{pct:.1f}%</td></tr>"
            )
    
    html_parts.extend([
        f"    <tr style='font-weight: bold; background: #E8F5E9;'><td>Gobernante</td><td>{data['governing']:.2f}</td><td>{data['governing'] * 35.3147:.0f}</td><td>100%</td></tr>",
        f"    <tr><td>Fugas</td><td>{data['leakage']:.2f}</td><td>{data['leakage'] * 35.3147:.0f}</td><td>-</td></tr>",
        f"    <tr style='font-weight: bold; background: #1B4332; color: white;'><td>TOTAL</td><td>{data['total']:.2f}</td><td>{data['total'] * 35.3147:.0f}</td><td>-</td></tr>",
        "  </table>",
    ])
    
    # Warnings
    warnings_list = getattr(result, 'warnings', []) if hasattr(result, 'warnings') else []
    if warnings_list:
        html_parts.append("  <h2>Advertencias</h2>")
        for w in warnings_list:
            html_parts.append(f"  <div class='warning'>{w}</div>")
    
    # Charts (if matplotlib available and requested)
    if include_charts and HAS_MATPLOTLIB:
        html_parts.append("  <h2>Graficos</h2>")
        
        # Generate bar chart
        try:
            fig_bar, _ = plot_flow_comparison(result, style=style)
            buf = io.BytesIO()
            fig_bar.savefig(buf, format='png', dpi=100, bbox_inches='tight')
            buf.seek(0)
            import base64
            img_data = base64.b64encode(buf.read()).decode('utf-8')
            plt.close(fig_bar)
            html_parts.append(f"  <div class='chart-container'><img src='data:image/png;base64,{img_data}' alt='Grafico de barras'></div>")
        except Exception as e:
            html_parts.append(f"  <p>Error generando grafico: {e}</p>")
        
        # Generate pie chart
        try:
            fig_pie, _ = plot_flow_breakdown(result, style=style, figsize=(6, 6))
            buf = io.BytesIO()
            fig_pie.savefig(buf, format='png', dpi=100, bbox_inches='tight')
            buf.seek(0)
            img_data = base64.b64encode(buf.read()).decode('utf-8')
            plt.close(fig_pie)
            html_parts.append(f"  <div class='chart-container'><img src='data:image/png;base64,{img_data}' alt='Grafico circular'></div>")
        except Exception as e:
            html_parts.append(f"  <p>Error generando grafico: {e}</p>")
    
    # Footer
    html_parts.extend([
        "  <div class='footer'>",
        "    <p><strong>Normativa:</strong> DS 024-2016-EM / DS 023-2017-EM (Peru)</p>",
        "    <p>Generado por VentPy - High-performance mine ventilation calculations</p>",
        "  </div>",
        "</div>",
        "</body>",
        "</html>",
    ])
    
    return '\n'.join(html_parts)


def save_report(
    result,
    filepath: str,
    title: str = "Reporte de Ventilacion",
    include_charts: bool = True,
) -> None:
    """
    Save an HTML report to a file.
    
    Args:
        result: VentilationDemandResult
        filepath: Output file path (should end in .html)
        title: Report title
        include_charts: Whether to embed charts
    """
    html = generate_html_report(result, title=title, include_charts=include_charts)
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(html)


# =============================================================================
# Quick Plot Functions
# =============================================================================

def quick_bar(result, **kwargs) -> Tuple[Any, Any]:
    """Quick bar chart with sensible defaults."""
    return plot_flow_comparison(result, **kwargs)


def quick_pie(result, **kwargs) -> Tuple[Any, Any]:
    """Quick pie chart with sensible defaults."""
    return plot_flow_breakdown(result, **kwargs)


def quick_dashboard(result, **kwargs) -> Tuple[Any, List[Any]]:
    """Quick dashboard with sensible defaults."""
    return create_dashboard(result, **kwargs)


# =============================================================================
# Utility Functions
# =============================================================================

def _check_matplotlib():
    """Check if matplotlib is available."""
    if not HAS_MATPLOTLIB:
        raise ImportError(
            "matplotlib is required for visualization. "
            "Install it with: pip install matplotlib"
        )


def _check_numpy():
    """Check if numpy is available."""
    if not HAS_NUMPY:
        raise ImportError(
            "numpy is required for this function. "
            "Install it with: pip install numpy"
        )


def show():
    """Display all current figures (wrapper for plt.show())."""
    _check_matplotlib()
    plt.show()


def savefig(filepath: str, **kwargs):
    """Save current figure to file (wrapper for plt.savefig())."""
    _check_matplotlib()
    plt.savefig(filepath, **kwargs)


# =============================================================================
# Module Exports
# =============================================================================

__all__ = [
    # Style
    'VentPyStyle',
    'DEFAULT_STYLE',
    # Main plotting functions
    'plot_flow_comparison',
    'plot_flow_breakdown',
    'plot_altitude_corrections',
    'create_dashboard',
    # Report generation
    'generate_html_report',
    'save_report',
    # Quick functions
    'quick_bar',
    'quick_pie',
    'quick_dashboard',
    # Utilities
    'show',
    'savefig',
    'extract_flow_data',
    'get_flow_labels',
]
