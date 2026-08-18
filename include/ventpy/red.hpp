/**
 * @file red.hpp
 * @brief Solver de red de ventilación — balance por Hardy Cross.
 *
 * Topología: ramales from→to; mallas detectadas automáticamente (árbol de
 * expansión BFS + cuerdas). La red se modela CERRADA (el nodo "superficie"
 * cierra admisión/retorno); un árbol sin mallas no tiene circulación y lanza.
 *
 * Algoritmo: McPherson (2009), Cap. 7 "Ventilation Network Analysis",
 * §7.2.1 (leyes de Kirchhoff) y §7.3.2 "The Hardy Cross Technique"
 * (pp. 7-13 a 7-19). Defaults de convergencia ingenieriles (NO normativos):
 * tolerancia 0.6 m³/min, 100 iteraciones.
 *
 * Capa: types → atmosphere → atkinson (física base) → red.
 * SIN safety_ceil: caudales de balance crudos (redondear rompería Kirchhoff).
 *
 * @copyright 2026 VentPy Project
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ventpy/atkinson.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

class NetworkSolver {
public:
    [[nodiscard]] static NetworkSolveResult solve(
        const NetworkDefinition& network,
        const AtmosphericParams& atm,
        const SolverParams& params = {}
    ) {
        validate(network, params);

        NetworkSolveResult result;
        result.biblio_ref =
            "McPherson (2009), Cap. 7, sec. 7.3.2 'The Hardy Cross Technique' "
            "(pp. 7-13 a 7-19); Kirchhoff sec. 7.2.1";

        // --- Resistencia por ramal (XOR ya validado) ---
        for (const NetworkBranch& b : network.branches) {
            BranchFlowResult br;
            br.branch_id = b.branch_id;
            br.from_node = b.from_node;
            br.to_node = b.to_node;
            br.fan_pressure_pa = b.fan_pressure_pa;
            br.r_ns2m8 = b.airway.has_value()
                ? AtkinsonCalculator::calculate_resistance(*b.airway, atm).r_total
                : b.r_manual;
            result.branches.push_back(std::move(br));
        }

        // --- Topología: nodos, conectividad, mallas ---
        std::set<std::string> nodes;
        for (const NetworkBranch& b : network.branches) {
            nodes.insert(b.from_node);
            nodes.insert(b.to_node);
        }
        result.node_count = static_cast<int>(nodes.size());
        require_connected(network, nodes);
        const int meshes = static_cast<int>(network.branches.size()) -
                           result.node_count + 1;
        if (meshes < 1) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: la red no tiene mallas (B - N + 1 = "
                + std::to_string(meshes) + "). Modela la red CERRADA: el nodo "
                "'superficie' debe cerrar el circuito de admision/retorno.");
        }
        result.mesh_count = meshes;

        // --- Estructuras internas de topología ---
        struct Edge { int idx; std::string from, to; };
        std::vector<Edge> edges;
        for (int i = 0; i < static_cast<int>(network.branches.size()); ++i) {
            edges.push_back({i, network.branches[i].from_node,
                                network.branches[i].to_node});
        }
        std::map<std::string, std::vector<int>> adj;
        for (const Edge& e : edges) {
            adj[e.from].push_back(e.idx);
            adj[e.to].push_back(e.idx);
        }

        // --- Árbol de expansión BFS (§7.3.2.a) ---
        std::set<int> tree_edges;
        std::map<std::string, int> parent_edge;   // nodo → edge que lo alcanzó
        std::set<std::string> visited{*nodes.begin()};
        std::deque<std::string> queue{*nodes.begin()};
        while (!queue.empty()) {
            const std::string node = queue.front(); queue.pop_front();
            for (int ei : adj[node]) {
                const Edge& e = edges[ei];
                const std::string& other = (e.from == node) ? e.to : e.from;
                if (visited.insert(other).second) {
                    tree_edges.insert(ei);
                    parent_edge[other] = ei;
                    queue.push_back(other);
                }
            }
        }

        // --- Mallas: una por cuerda; camino en el árbol de to→from de la cuerda ---
        // signo +1 = el ramal se recorre from→to dentro de la malla
        std::vector<std::vector<std::pair<int, double>>> mesh_list;
        for (const Edge& chord : edges) {
            if (tree_edges.count(chord.idx)) continue;
            std::vector<std::pair<int, double>> mesh{{chord.idx, +1.0}};
            // camino árbol: chord.to → chord.from (BFS restringido al árbol)
            std::map<std::string, std::pair<std::string, int>> prev;
            std::deque<std::string> q2{chord.to};
            prev[chord.to] = {chord.to, -1};
            while (!q2.empty()) {
                const std::string node = q2.front(); q2.pop_front();
                if (node == chord.from) break;
                for (int ei : adj[node]) {
                    if (!tree_edges.count(ei)) continue;
                    const Edge& e = edges[ei];
                    const std::string& other = (e.from == node) ? e.to : e.from;
                    if (!prev.count(other)) {
                        prev[other] = {node, ei};
                        q2.push_back(other);
                    }
                }
            }
            std::string node = chord.from;
            while (prev[node].second != -1) {
                const auto& [pnode, ei] = prev[node];
                const Edge& e = edges[ei];
                // recorrido pnode→node; signo + si coincide con from→to del ramal
                mesh.push_back({ei, (e.from == pnode) ? +1.0 : -1.0});
                node = pnode;
            }
            mesh_list.push_back(std::move(mesh));
        }

        // --- Inicialización (§7.3.2.b): cuerdas = q_initial o 60 m³/min;
        //     árbol por continuidad (Kirchhoff I, pelado de nodos con 1 incógnita) ---
        std::vector<double> q(edges.size(), 0.0);   // m³/s, + = from→to
        std::set<int> unknown(tree_edges.begin(), tree_edges.end());
        for (const Edge& e : edges) {
            if (tree_edges.count(e.idx)) continue;
            const double q0 = network.branches[e.idx].q_initial_m3min;
            q[e.idx] = (q0 > 0.0 ? q0 : 60.0) / 60.0;
        }
        while (!unknown.empty()) {
            bool progressed = false;
            for (const std::string& node : nodes) {
                std::vector<int> unk;
                for (int ei : adj[node]) if (unknown.count(ei)) unk.push_back(ei);
                if (unk.size() != 1) continue;
                double inflow = 0.0;
                for (int ei : adj[node]) {
                    if (unknown.count(ei)) continue;
                    inflow += (edges[ei].to == node) ? q[ei] : -q[ei];
                }
                const Edge& e = edges[unk[0]];
                q[unk[0]] = (e.from == node) ? inflow : -inflow;
                unknown.erase(unk[0]);
                progressed = true;
            }
            if (!progressed) {
                // no debería ocurrir en red conexa; defensa explícita
                throw std::logic_error(
                    "[VentPy] NetworkSolver: fallo interno inicializando "
                    "continuidad (reportar como bug).");
            }
        }

        // --- Iteración Hardy Cross (§7.3.2.c-d) ---
        const double tol_m3s = params.tolerance_m3min / 60.0;
        constexpr double DEN_FLOOR = 1e-12;   // piso anti división-por-cero
        double max_dq = 0.0;
        int iter = 0;
        for (; iter < params.max_iterations; ) {
            ++iter;
            max_dq = 0.0;
            for (const auto& mesh : mesh_list) {
                double num = 0.0, den = 0.0;
                for (const auto& [ei, sign] : mesh) {
                    const double qm = q[ei] * sign;   // caudal en sentido de malla
                    num += result.branches[ei].r_ns2m8 * qm * std::abs(qm);
                    num -= sign * result.branches[ei].fan_pressure_pa;
                    den += 2.0 * result.branches[ei].r_ns2m8 * std::abs(qm);
                }
                const double dq = -num / std::max(den, DEN_FLOOR);
                max_dq = std::max(max_dq, std::abs(dq));
                for (const auto& [ei, sign] : mesh) q[ei] += sign * dq;
            }
            if (max_dq <= tol_m3s) { result.converged = true; break; }
        }
        result.iterations = iter;
        result.max_residual_m3min = max_dq * 60.0;
        if (!result.converged) {
            std::ostringstream oss;
            oss << "NO CONVERGIO en " << iter << " iteraciones (residual "
                << result.max_residual_m3min << " m3/min > tolerancia "
                << params.tolerance_m3min << "). Resultados NO confiables.";
            result.warnings.push_back(oss.str());
        }

        // --- Volcado a resultado (Q y ΔP con signo; crudos) ---
        for (size_t i = 0; i < edges.size(); ++i) {
            result.branches[i].q_m3min = q[i] * 60.0;
            result.branches[i].pressure_drop_pa =
                result.branches[i].r_ns2m8 * q[i] * std::abs(q[i]);
        }

        // --- Post-proceso: velocidad por ramal + advertencia Art. 248 ---
        for (size_t i = 0; i < network.branches.size(); ++i) {
            const NetworkBranch& src = network.branches[i];
            if (src.airway.has_value() && src.airway->area_m2 > 0.0) {
                const double v_mps = std::abs(q[i]) / src.airway->area_m2;
                result.branches[i].velocity_mps = v_mps;
                const double v_mpm = v_mps * 60.0;
                // DS 024-2016-EM, Art. 248 (misma cita que cobertura/atkinson)
                if (std::abs(result.branches[i].q_m3min) > 0.0 &&
                    (v_mpm < 20.0 || v_mpm > 250.0)) {
                    std::ostringstream oss;
                    oss << "Ramal '" << src.branch_id << "': velocidad " << v_mpm
                        << " m/min fuera del rango [20, 250] (DS 024-2016-EM, Art. 248)";
                    result.branches[i].warnings.push_back(oss.str());
                    result.warnings.push_back(result.branches[i].warnings.back());
                }
            }
        }

        return result;
    }

private:
    static void validate(const NetworkDefinition& n, const SolverParams& p) {
        if (n.branches.empty()) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: la red no tiene ramales.");
        }
        validation::require_positive(p.tolerance_m3min,
            "tolerance_m3min [m3/min] - Tolerancia de convergencia");
        validation::require_positive_int(p.max_iterations,
            "max_iterations - Iteraciones maximas");
        std::set<std::string> ids;
        for (const NetworkBranch& b : n.branches) {
            if (b.branch_id.empty() || b.from_node.empty() || b.to_node.empty()) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: branch_id/from_node/to_node "
                    "no pueden ser vacios.");
            }
            if (!ids.insert(b.branch_id).second) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: branch_id duplicado: '" +
                    b.branch_id + "'.");
            }
            if (b.from_node == b.to_node) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: self-loop no soportado (ramal '" +
                    b.branch_id + "').");
            }
            const bool has_airway = b.airway.has_value();
            const bool has_manual = b.r_manual > 0.0;
            if (has_airway == has_manual) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: ramal '" + b.branch_id +
                    "': la resistencia debe tener exactamente una fuente "
                    "(airway O r_manual > 0).");
            }
            validation::require_non_negative(b.fan_pressure_pa,
                "fan_pressure_pa [Pa] - Presion de ventilador");
            validation::require_non_negative(b.q_initial_m3min,
                "q_initial_m3min [m3/min] - Caudal inicial");
        }
    }

    static void require_connected(const NetworkDefinition& n,
                                  const std::set<std::string>& nodes) {
        std::map<std::string, std::vector<const NetworkBranch*>> adj;
        for (const NetworkBranch& b : n.branches) {
            adj[b.from_node].push_back(&b);
            adj[b.to_node].push_back(&b);
        }
        std::set<std::string> visited;
        std::deque<std::string> queue{*nodes.begin()};
        visited.insert(*nodes.begin());
        while (!queue.empty()) {
            const std::string node = queue.front();
            queue.pop_front();
            for (const NetworkBranch* b : adj[node]) {
                const std::string& other =
                    (b->from_node == node) ? b->to_node : b->from_node;
                if (visited.insert(other).second) queue.push_back(other);
            }
        }
        if (visited.size() != nodes.size()) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: la red no es conexa (" +
                std::to_string(visited.size()) + " de " +
                std::to_string(nodes.size()) + " nodos alcanzables).");
        }
    }
};

} // namespace ventpy
