/*PGR-GNU*****************************************************************
File: many_to_dist_driving_distance.c

Copyright (c) 2015 Celia Virginia Vergara Castillo
Mail: vicky_Vergara@hotmail.com

------

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 ********************************************************************PGR-GNU*/

#include "./../../common/src/postgres_connection.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "funcapi.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "utils/array.h"

#include "./../../common/src/debug_macro.h"
#include "./../../common/src/time_msg.h"
#include "./../../common/src/pgr_types.h"
#include "./../../common/src/edges_input.h"
#include "./../../common/src/arrays_input.h"
#include "./drivedist_driver.h"


PGDLLEXPORT Datum driving_many_to_dist(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(driving_many_to_dist);


static 
void driving_many_to_dist_driver(
        char* sql,
        int64_t *start_vertex, size_t num,
        float8 distance,
        bool directed,
        bool equicost, 
        General_path_element_t **path, size_t *path_count) {
    pgr_SPI_connect();
    pgr_edge_t *edges = NULL;
    size_t total_tuples = 0;


    char *err_msg = (char *)"";


    pgr_get_edges(sql, &edges, &total_tuples);

    if (total_tuples == 0) {
        PGR_DBG("No edges found");
        (*path_count) = 0;
        *path = NULL;
        return;
    }

    clock_t start_t = clock();
    do_pgr_driving_many_to_dist(
            edges, total_tuples,
            start_vertex, num,
            distance,
            directed,
            equicost,
            path, path_count,
            &err_msg);
    time_msg(" processing DrivingDistance many starts", start_t, clock());

    pfree(edges);
    pgr_SPI_finish(); 
}


PGDLLEXPORT Datum
driving_many_to_dist(PG_FUNCTION_ARGS) {
    FuncCallContext     *funcctx;
    TupleDesc            tuple_desc;
    General_path_element_t  *ret_path = 0;

    /* stuff done only on the first call of the function */
    if (SRF_IS_FIRSTCALL()) {
        MemoryContext   oldcontext;
        size_t path_count = 0;

        /* create a function context for cross-call persistence */
        funcctx = SRF_FIRSTCALL_INIT();

        /* switch to memory context appropriate for multiple function calls */
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        int64_t* sourcesArr = NULL;
        size_t num = 0;

        sourcesArr = (int64_t*) pgr_get_bigIntArray(&num, PG_GETARG_ARRAYTYPE_P(1));
        PGR_DBG("sourcesArr size %ld ", num);

        PGR_DBG("Calling driving_many_to_dist_driver");
        driving_many_to_dist_driver(
                text_to_cstring(PG_GETARG_TEXT_P(0)),  // sql
                sourcesArr, num,                     // array of sources
                PG_GETARG_FLOAT8(2),                 // distance
                PG_GETARG_BOOL(3),                   // directed
                PG_GETARG_BOOL(4),                   // equicost
                &ret_path, &path_count);

        pfree(sourcesArr);

        /* total number of tuples to be returned */
        funcctx->max_calls = (uint32_t) path_count;
        funcctx->user_fctx = ret_path;
        if (get_call_result_type(fcinfo, NULL, &tuple_desc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("function returning record called in context "
                         "that cannot accept type record")));

        funcctx->tuple_desc = tuple_desc;

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();

    tuple_desc = funcctx->tuple_desc;
    ret_path = (General_path_element_t*) funcctx->user_fctx;

    if (funcctx->call_cntr < funcctx->max_calls) {
        HeapTuple    tuple;
        Datum        result;
        Datum *values;
        bool* nulls;

        values = palloc(6 * sizeof(Datum));
        nulls = palloc(6 * sizeof(bool));
        // id, start_v, node, edge, cost, tot_cost
        nulls[0] = false;
        nulls[1] = false;
        nulls[2] = false;
        nulls[3] = false;
        nulls[4] = false;
        nulls[5] = false;
        values[0] = Int32GetDatum(funcctx->call_cntr + 1);
        values[1] = Int64GetDatum(ret_path[funcctx->call_cntr].start_id);
        values[2] = Int64GetDatum(ret_path[funcctx->call_cntr].node);
        values[3] = Int64GetDatum(ret_path[funcctx->call_cntr].edge);
        values[4] = Float8GetDatum(ret_path[funcctx->call_cntr].cost);
        values[5] = Float8GetDatum(ret_path[funcctx->call_cntr].agg_cost);

        tuple = heap_form_tuple(tuple_desc, values, nulls);
        result = HeapTupleGetDatum(tuple);

        pfree(values);
        pfree(nulls);

        SRF_RETURN_NEXT(funcctx, result);
    } else {
        SRF_RETURN_DONE(funcctx);
    }
}

