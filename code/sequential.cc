/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include "sequential.hh"
#include "bit_graph.hh"
#include "template_voodoo.hh"
#include "degree_sort.hh"

#include <algorithm>
#include <numeric>
#include <limits>
#include <random>

namespace
{
    enum class Search
    {
        Aborted,
        Unsatisfiable,
        Satisfiable
    };

    template <unsigned n_words_, int k_, int l_>
    struct SequentialSubgraphIsomorphism
    {
        struct Domain
        {
            unsigned v;
            unsigned popcount;
            FixedBitSet<n_words_> values;
        };

        using Domains = std::vector<Domain>;
        using Assignments = std::vector<unsigned>;

        const Params & params;

        static constexpr int max_graphs = 1 + (l_ - 1) * k_;
        std::array<FixedBitGraph<n_words_>, max_graphs> target_graphs;
        std::array<FixedBitGraph<n_words_>, max_graphs> pattern_graphs;

        std::vector<int> pattern_order, target_order, isolated_vertices;
        std::array<std::pair<int, int>, n_words_ * bits_per_word> pattern_degree_tiebreak;

        unsigned pattern_size, full_pattern_size, target_size;

        SequentialSubgraphIsomorphism(const Graph & target, const Graph & pattern, const Params & a) :
            params(a),
            target_order(target.size()),
            pattern_size(pattern.size()),
            full_pattern_size(pattern.size()),
            target_size(target.size())
        {
            // strip out isolated vertices in the pattern
            for (unsigned v = 0 ; v < full_pattern_size ; ++v)
                if (0 == pattern.degree(v)) {
                    isolated_vertices.push_back(v);
                    --pattern_size;
                }
                else
                    pattern_order.push_back(v);

            // recode pattern to a bit graph
            pattern_graphs.at(0).resize(pattern_size);
            for (unsigned i = 0 ; i < pattern_size ; ++i)
                for (unsigned j = 0 ; j < pattern_size ; ++j)
                    if (pattern.adjacent(pattern_order.at(i), pattern_order.at(j)))
                        pattern_graphs.at(0).add_edge(i, j);

            // determine ordering for target graph vertices
            std::iota(target_order.begin(), target_order.end(), 0);
            degree_sort(target, target_order, false);

            // recode target to a bit graph
            target_graphs.at(0).resize(target_size);
            for (unsigned i = 0 ; i < target_size ; ++i)
                for (unsigned j = 0 ; j < target_size ; ++j)
                    if (target.adjacent(target_order.at(i), target_order.at(j)))
                        target_graphs.at(0).add_edge(i, j);

            for (unsigned j = 0 ; j < pattern_size ; ++j)
                pattern_degree_tiebreak.at(j).first = pattern_graphs.at(0).degree(j);
            for (unsigned i = 0 ; i < pattern_size ; ++i)
                for (unsigned j = 0 ; j < pattern_size ; ++j)
                    if (pattern_graphs.at(0).adjacent(i, j))
                        pattern_degree_tiebreak.at(j).second += pattern_degree_tiebreak.at(i).first;
        }

        auto build_supplemental_graphs() -> void
        {
            for (int g = 1 ; g < max_graphs ; ++g)
                pattern_graphs.at(g).resize(pattern_size);

            if (l_ >= 2) {
                for (unsigned v = 0 ; v < pattern_size ; ++v) {
                    auto nv = pattern_graphs.at(0).neighbourhood(v);
                    for (int c = nv.first_set_bit() ; c != -1 ; c = nv.first_set_bit()) {
                        nv.unset(c);
                        auto nc = pattern_graphs.at(0).neighbourhood(c);
                        for (int w = nc.first_set_bit() ; w != -1 && unsigned(w) <= v ; w = nc.first_set_bit()) {
                            nc.unset(w);
                            if (k_ >= 5 && pattern_graphs.at(4).adjacent(v, w))
                                pattern_graphs.at(5).add_edge(v, w);
                            else if (k_ >= 4 && pattern_graphs.at(3).adjacent(v, w))
                                pattern_graphs.at(4).add_edge(v, w);
                            else if (k_ >= 3 && pattern_graphs.at(2).adjacent(v, w))
                                pattern_graphs.at(3).add_edge(v, w);
                            else if (k_ >= 2 && pattern_graphs.at(1).adjacent(v, w))
                                pattern_graphs.at(2).add_edge(v, w);
                            else if (k_ >= 1)
                                pattern_graphs.at(1).add_edge(v, w);
                        }
                    }
                }
            }

            for (int g = 1 ; g < max_graphs ; ++g)
                target_graphs.at(g).resize(target_size);

            if (l_ >= 2) {
                for (unsigned v = 0 ; v < target_size ; ++v) {
                    auto nv = target_graphs.at(0).neighbourhood(v);
                    for (int c = nv.first_set_bit() ; c != -1 ; c = nv.first_set_bit()) {
                        nv.unset(c);
                        auto nc = target_graphs.at(0).neighbourhood(c);
                        for (int w = nc.first_set_bit() ; w != -1 && unsigned(w) <= v ; w = nc.first_set_bit()) {
                            nc.unset(w);
                            if (k_ >= 5 && target_graphs.at(4).adjacent(v, w))
                                target_graphs.at(5).add_edge(v, w);
                            else if (k_ >= 4 && target_graphs.at(3).adjacent(v, w))
                                target_graphs.at(4).add_edge(v, w);
                            else if (k_ >= 3 && target_graphs.at(2).adjacent(v, w))
                                target_graphs.at(3).add_edge(v, w);
                            else if (k_ >= 2 && target_graphs.at(1).adjacent(v, w))
                                target_graphs.at(2).add_edge(v, w);
                            else if (k_ >= 1)
                                target_graphs.at(1).add_edge(v, w);
                        }
                    }
                }
            }
        }

        auto assign(Domains & new_domains, unsigned branch_v, unsigned f_v, int g_end) -> bool
        {
            // for each remaining domain...
            for (auto & d : new_domains) {
                // all different
                d.values.unset(f_v);

                // for each graph pair...
                for (int g = 0 ; g < g_end ; ++g) {
                    // if we're adjacent...
                    if (pattern_graphs.at(g).adjacent(branch_v, d.v)) {
                        // ...then we can only be mapped to adjacent vertices
                        target_graphs.at(g).intersect_with_row(f_v, d.values);
                    }
                }

                // we might have removed values
                d.popcount = d.values.popcount();
                if (0 == d.popcount)
                    return false;
            }

            if (! cheap_all_different(new_domains))
                return false;

            return true;
        }

        auto search(
                Assignments & assignments,
                Domains & domains,
                unsigned long long & nodes,
                int g_end, int depth) -> Search
        {
            if (params.abort->load())
                return Search::Aborted;

            ++nodes;

            Domain * branch_domain = nullptr;
            for (auto & d : domains)
                if ((! branch_domain) ||
                        d.popcount < branch_domain->popcount ||
                        (d.popcount == branch_domain->popcount && pattern_degree_tiebreak.at(d.v) > pattern_degree_tiebreak.at(branch_domain->v)))
                    branch_domain = &d;

            if (! branch_domain)
                return Search::Satisfiable;

            auto remaining = branch_domain->values;
            auto branch_v = branch_domain->v;

            for (int f_v = remaining.first_set_bit() ; f_v != -1 ; f_v = remaining.first_set_bit()) {
                remaining.unset(f_v);

                /* try assigning f_v to v */
                assignments.at(branch_v) = f_v;

                /* set up new domains */
                Domains new_domains;
                new_domains.reserve(domains.size() - 1);
                for (auto & d : domains)
                    if (d.v != branch_v)
                        new_domains.push_back(d);

                /* assign and propagate */
                if (! assign(new_domains, branch_v, f_v, g_end))
                    continue;

                auto search_result = search(assignments, new_domains, nodes, g_end, depth + 1);
                switch (search_result) {
                    case Search::Satisfiable:    return Search::Satisfiable;
                    case Search::Aborted:        return Search::Aborted;
                    case Search::Unsatisfiable:  break;
                }
            }

            return Search::Unsatisfiable;
        }

        auto initialise_domains(Domains & domains, int g_end) -> bool
        {
            std::array<std::vector<int>, max_graphs> patterns_degrees;
            std::array<std::vector<int>, max_graphs> targets_degrees;

            for (int g = 0 ; g < g_end ; ++g) {
                patterns_degrees.at(g).resize(pattern_size);
                targets_degrees.at(g).resize(target_size);
            }

            /* pattern and target degree sequences */
            for (int g = 0 ; g < g_end ; ++g) {
                for (unsigned i = 0 ; i < pattern_size ; ++i)
                    patterns_degrees.at(g).at(i) = pattern_graphs.at(g).degree(i);

                for (unsigned i = 0 ; i < target_size ; ++i)
                    targets_degrees.at(g).at(i) = target_graphs.at(g).degree(i);
            }

            /* pattern and target neighbourhood degree sequences */
            std::array<std::vector<std::vector<int> >, max_graphs> patterns_ndss;
            std::array<std::vector<std::vector<int> >, max_graphs> targets_ndss;

            for (int g = 0 ; g < g_end ; ++g) {
                patterns_ndss.at(g).resize(pattern_size);
                targets_ndss.at(g).resize(target_size);
            }

            for (int g = 0 ; g < g_end ; ++g) {
                for (unsigned i = 0 ; i < pattern_size ; ++i) {
                    for (unsigned j = 0 ; j < pattern_size ; ++j) {
                        if (pattern_graphs.at(g).adjacent(i, j))
                            patterns_ndss.at(g).at(i).push_back(patterns_degrees.at(g).at(j));
                    }
                    std::sort(patterns_ndss.at(g).at(i).begin(), patterns_ndss.at(g).at(i).end(), std::greater<int>());
                }

                for (unsigned i = 0 ; i < target_size ; ++i) {
                    for (unsigned j = 0 ; j < target_size ; ++j) {
                        if (target_graphs.at(g).adjacent(i, j))
                            targets_ndss.at(g).at(i).push_back(targets_degrees.at(g).at(j));
                    }
                    std::sort(targets_ndss.at(g).at(i).begin(), targets_ndss.at(g).at(i).end(), std::greater<int>());
                }
            }

            for (unsigned i = 0 ; i < pattern_size ; ++i) {
                domains.at(i).v = i;
                domains.at(i).values.unset_all();

                for (unsigned j = 0 ; j < target_size ; ++j) {
                    bool ok = true;

                    for (int g = 0 ; g < g_end ; ++g) {
                        if (pattern_graphs.at(g).adjacent(i, i) && ! target_graphs.at(g).adjacent(j, j))
                            ok = false;
                        else if (targets_ndss.at(g).at(j).size() < patterns_ndss.at(g).at(i).size())
                            ok = false;
                        else {
                            for (unsigned x = 0 ; ok && x < patterns_ndss.at(g).at(i).size() ; ++x) {
                                if (targets_ndss.at(g).at(j).at(x) < patterns_ndss.at(g).at(i).at(x))
                                    ok = false;
                            }
                        }

                        if (! ok)
                            break;
                    }

                    if (ok)
                        domains.at(i).values.set(j);
                }

                domains.at(i).popcount = domains.at(i).values.popcount();
            }

            FixedBitSet<n_words_> domains_union;
            for (auto & d : domains)
                domains_union.union_with(d.values);

            unsigned domains_union_popcount = domains_union.popcount();
            if (domains_union_popcount < unsigned(pattern_size))
                return false;

            return true;
        }

        auto cheap_all_different(Domains & domains) -> bool
        {
            // pick domains smallest first, with tiebreaking
            std::array<int, n_words_ * bits_per_word> domains_order;
            std::iota(domains_order.begin(), domains_order.begin() + domains.size(), 0);

            std::sort(domains_order.begin(), domains_order.begin() + domains.size(),
                    [&] (int a, int b) {
                    return (domains.at(a).popcount < domains.at(b).popcount) ||
                    (domains.at(a).popcount == domains.at(b).popcount && pattern_degree_tiebreak.at(domains.at(a).v) > pattern_degree_tiebreak.at(domains.at(b).v));
                    });

            // counting all-different
            FixedBitSet<n_words_> domains_so_far, hall;
            unsigned neighbours_so_far = 0;

            for (int i = 0, i_end = domains.size() ; i != i_end ; ++i) {
                auto & d = domains.at(domains_order.at(i));

                d.values.intersect_with_complement(hall);
                d.popcount = d.values.popcount();

                if (0 == d.popcount)
                    return false;

                domains_so_far.union_with(d.values);
                ++neighbours_so_far;

                unsigned domains_so_far_popcount = domains_so_far.popcount();
                if (domains_so_far_popcount < neighbours_so_far) {
                    return false;
                }
                else if (domains_so_far_popcount == neighbours_so_far) {
                    neighbours_so_far = 0;
                    hall.union_with(domains_so_far);
                    domains_so_far.unset_all();
                }
            }

            return true;
        }

        auto prepare_for_search(Domains & domains) -> void
        {
            for (auto & d : domains)
                d.popcount = d.values.popcount();
        }

        auto save_result(Assignments & assignments, Result & result) -> void
        {
            for (unsigned v = 0 ; v < pattern_size ; ++v)
                result.isomorphism.emplace(pattern_order.at(v), target_order.at(assignments.at(v)));

            int t = 0;
            for (auto & v : isolated_vertices) {
                while (result.isomorphism.end() != std::find_if(result.isomorphism.begin(), result.isomorphism.end(),
                            [&t] (const std::pair<int, int> & p) { return p.second == t; }))
                        ++t;
                result.isomorphism.emplace(v, t);
            }
        }

        auto run() -> Result
        {
            Result result;

            if (full_pattern_size > target_size) {
                /* some of our fixed size data structures will throw a hissy
                 * fit. check this early. */
                return result;
            }

            build_supplemental_graphs();

            Domains domains(pattern_size);

            if (! initialise_domains(domains, max_graphs))
                return result;

            if (! cheap_all_different(domains))
                return result;

            prepare_for_search(domains);

            Assignments assignments(pattern_size, std::numeric_limits<unsigned>::max());
            switch (search(assignments, domains, result.nodes, max_graphs, 0)) {
                case Search::Satisfiable:
                    save_result(assignments, result);
                    break;

                case Search::Unsatisfiable:
                    break;

                case Search::Aborted:
                    break;
            }

            return result;
        }
    };

    template <template <unsigned, int, int> class SGI_, int n_, int m_>
    struct Apply
    {
        template <unsigned size_, typename> using Type = SGI_<size_, n_, m_>;
    };
}

auto sequential_subgraph_isomorphism(const std::pair<Graph, Graph> & graphs, const Params & params) -> Result
{
    if (graphs.first.size() > graphs.second.size())
        return Result{ };
    return select_graph_size<Apply<SequentialSubgraphIsomorphism, 4, 2>::template Type, Result>(
            AllGraphSizes(), graphs.second, graphs.first, params);
}

