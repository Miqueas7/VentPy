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
        std::vector<double> r_vals;
        for (const NetworkBranch& b : network.branches) {
            BranchFlowResult br;
            br.branch_id = b.branch_id;
            br.from_node = b.from_node;
            br.to_node = b.to_node;
            br.fan_pressure_pa = b.fan_pressure_pa;
            br.r_ns2m8 = b.airway.has_value()
                ? AtkinsonCalculator::calculate_resistance(*b.airway, atm).r_total
                : b.r_manual;
            r_vals.push_back(br.r_ns2m8);
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

        // Task 2 implementa la iteración; stub deliberado:
        result.converged = false;
        result.warnings.push_back("[stub] iteracion Hardy Cross pendiente (Task 2)");
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
