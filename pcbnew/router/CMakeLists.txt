include_directories( BEFORE ${INC_BEFORE} )

include_directories(
    ./
    ../
    ../../include
    ../../pcbnew
    ../../polygon
    ${INC_AFTER}
)

set( PCBNEW_PNS_SRCS
    direction.h
    ranged_num.h
    range.h
    time_limit.cpp
    time_limit.h
    trace.h

    pns_algo_base.cpp
    pns_algo_base.h
    pns_diff_pair.cpp
    pns_diff_pair.h
    pns_diff_pair_placer.cpp
    pns_diff_pair_placer.h
    pns_dp_meander_placer.cpp
    pns_dp_meander_placer.h
    pns_dragger.cpp
    pns_dragger.h
    pns_index.h
    pns_item.cpp
    pns_item.h
    pns_itemset.cpp
    pns_itemset.h
    pns_joint.h
    pns_layerset.h
    pns_line.cpp
    pns_line.h
    pns_line_placer.cpp
    pns_line_placer.h
    pns_logger.cpp
    pns_logger.h
    pns_meander.cpp
    pns_meander.h
    pns_meander_placer_base.cpp
    pns_meander_placer_base.h
    pns_meander_placer.cpp
    pns_meander_placer.h
    pns_meander_skew_placer.cpp
    pns_meander_skew_placer.h
    pns_node.cpp
    pns_node.h
    pns_optimizer.cpp
    pns_optimizer.h
    pns_placement_algo.h
    pns_router.cpp
    pns_router.h
    pns_routing_settings.cpp
    pns_routing_settings.h
    pns_segment.h
    pns_shove.cpp
    pns_shove.h
    pns_sizes_settings.cpp
    pns_sizes_settings.h
    pns_solid.cpp
    pns_solid.h
    pns_tool_base.cpp
    pns_tool_base.h
    pns_topology.cpp
    pns_topology.h
    pns_tune_status_popup.cpp
    pns_tune_status_popup.h
    pns_utils.cpp
    pns_utils.h
    pns_via.cpp
    pns_via.h
    pns_walkaround.cpp
    pns_walkaround.h

    router_preview_item.cpp
    router_preview_item.h
    router_tool.cpp
    router_tool.h
    length_tuner_tool.cpp
    length_tuner_tool.h
)

add_library( pnsrouter STATIC ${PCBNEW_PNS_SRCS} )
