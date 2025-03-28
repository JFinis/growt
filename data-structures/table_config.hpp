#pragma once

#include <sstream>
#include <type_traits>

#include "data-structures/element_types/complex_slot.hpp"
#include "data-structures/element_types/simple_slot.hpp"
#include "data-structures/element_types/single_word_slot.hpp"

#include "data-structures/strategies/estrat_async.hpp"
#include "data-structures/strategies/estrat_sync.hpp"
#include "data-structures/strategies/wstrat_pool.hpp"
#include "data-structures/strategies/wstrat_user.hpp"

#include "data-structures/base_linear.hpp"
#include "data-structures/hash_table_mods.hpp"
#include "data-structures/migration_table.hpp"

namespace growt
{

enum class slot_type_enum
{
    complex_slot     = 1,
    simple_slot      = 2,
    single_word_slot = 3
};

template <size_t, size_t, bool, typename Alloc>
struct slot_config
{
    template <class K, class M, bool NM>
    using templ = complex_slot<K, M, NM, Alloc>;
};

template <typename Alloc>
struct slot_config<16, 8, false, Alloc>
{
    template <class K, class M, bool NM>
    using templ = simple_slot<K, M, NM>;
};


template <typename Alloc>
struct slot_config<8, 4, false, Alloc>
{
    template <class K, class M, bool NM>
    using templ = single_word_slot<K, M, NM>;
};

template <class Key, class Data, class HashFct, class Allocator, hmod... Mods>
class table_config
{
  public:
    // INPUT TYPES
    using key_type       = Key;
    using mapped_type    = Data;
    using hash_fct_type  = HashFct;
    using allocator_type = Allocator;

    using mods = mod_aggregator<Mods...>;

    // DERIVED TYPES
    using value_type = std::pair<const key_type, mapped_type>;


    static constexpr bool needs_marking = mods::template is<hmod::growable>() &&
                                          !(mods::template is<hmod::sync>());
    static constexpr bool needs_growing_with_ref_integrity =
        mods::template is<hmod::ref_integrity>() &&
        mods::template is<hmod::growable>();
    static constexpr bool needs_migration =
        mods::template is<hmod::growable>() ||
        mods::template is<hmod::deletion>();
    // template <class K, class M, bool NM>
    // using slot_config    = typename template_conditional<needs_complex_slot,
    //                                                      complex_slot,
    //                                                      simple_slot>::template
    //                                                      templ<K,M,NM>;

    using base_table_config = base_linear_config<
        typename slot_config<sizeof(value_type),
                             sizeof(key_type),
                             needs_growing_with_ref_integrity, allocator_type>::
            template templ<key_type, mapped_type, needs_marking>,
        HashFct,
        Allocator,
        mods::template is<hmod::circular_map>(),
        mods::template is<hmod::circular_prob>(),
        !mods::template is<hmod::growable>()>;

    using base_table_type = base_linear<base_table_config>;

    template <class P>
    using workerstrat =
        typename std::conditional<!mods::template is<hmod::pool>(),
                                  wstrat_user<P>,
                                  wstrat_pool<P> >::type;
    template <class P>
    using exclstrat =
        typename std::conditional<!mods::template is<hmod::sync>(),
                                  estrat_async<P>,
                                  estrat_sync<P> >::type;



    using table_type = typename std::conditional<
        needs_migration,
        migration_table<base_table_type, workerstrat, exclstrat>,
        base_table_type>::type;


    static std::string name() { return table_type::name(); }
};

} // namespace growt
