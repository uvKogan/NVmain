/*******************************************************************************
* MBMM T1.4: Start-Gap wear-leveling decoder for NVMain 2.0.
* See StartGap.h for the algorithm summary and references.
*******************************************************************************/
#include "Decoders/StartGap/StartGap.h"
#include <iostream>

using namespace NVM;

uint64_t StartGap::start = 0;
uint64_t StartGap::gap = 0;
uint64_t StartGap::writesSinceMove = 0;
uint64_t StartGap::lines = 0;
uint64_t StartGap::interval = 100;
ncounter_t StartGap::startGapMoves = 0;
ncounter_t StartGap::startGapWrites = 0;
ncounter_t StartGap::startGapOutOfRegion = 0;
ncounter_t StartGap::startGapBoundaryAlias = 0;

StartGap::StartGap( ) { }
StartGap::~StartGap( ) { }

void StartGap::SetConfig( Config *config, bool createChildren )
{
    AddressTranslator::SetConfig( config, createChildren );

    if( config->KeyExists( "StartGapInterval" ) )
        interval = static_cast<uint64_t>( config->GetValue( "StartGapInterval" ) );

    /* Region size is derived once (six decoder instances all call SetConfig
     * with equivalent global config keys, so this is idempotent). lines is
     * the *physical* line count == N + 1; the gap starts at the region's
     * last physical line (GAP = N) and START at 0, matching Qureshi Fig. 4's
     * initial state where the spare line trails the data. */
    if( lines == 0 )
    {
        uint64_t bytes;
        if( config->KeyExists( "StartGapRegionBytes" ) )
            bytes = static_cast<uint64_t>( config->GetValue( "StartGapRegionBytes" ) );
        else
            bytes = static_cast<uint64_t>( config->GetValue( "ROWS" ) ) * config->GetValue( "COLS" ) * 64
                   * config->GetValue( "BANKS" ) * config->GetValue( "RANKS" );

        lines = bytes / 64;
        if( lines < 2 )
            lines = 2;                    /* need at least N=1 data line + 1 gap line */

        gap = lines - 1;                  /* GAP = N */
        start = 0;
    }
}

/*
 * Remap(): forward Start-Gap map, Qureshi et al. MICRO 2009 Fig. 4.
 *   N = lines - 1 logical (data) lines map onto the N+1 physical lines.
 *   s      = (LA + START) mod N
 *   PA     = (s < GAP) ? s : s + 1        -- skips exactly the GAP slot
 * The region's very last physical line (LA == N, i.e. the one line beyond
 * the N logical/data lines) and anything at or beyond the region boundary
 * pass through unchanged and are counted in startGapOutOfRegion; they are
 * never wear-leveled.
 *
 * LA == N is additionally counted in startGapBoundaryAlias (on top of
 * startGapOutOfRegion): physical line N is also a legitimate Map() target
 * for real data whenever GAP < N, so this specific boundary line silently
 * aliases with whatever data currently lands on physical line N. Accepted
 * with visibility per the review ruling rather than redesigned -- see the
 * header comment. A nonzero startGapBoundaryAlias on a run flags that
 * run's wear numbers as touching the aliased line.
 */
uint64_t StartGap::Remap( uint64_t address )
{
    uint64_t line = address / 64, off = address % 64;
    uint64_t N = lines - 1;

    if( line >= N )
    {
        startGapOutOfRegion++;
        if( line == N )
            startGapBoundaryAlias++;
        return address;
    }

    uint64_t s = ( line + start ) % N;
    uint64_t mapped = ( s < gap ) ? s : s + 1;
    return mapped * 64 + off;
}

/*
 * Unmap(): exact inverse of Remap() for physical lines in [0, N] that were
 * produced by it (i.e. PA != GAP for legitimate data). PA == GAP cannot
 * arise from real data (Remap() never returns GAP) but ReverseTranslate()
 * can in principle synthesize an ACT/PRE/REF address that lands on the
 * gap slot (e.g. while GAP is mid-flight relative to the request that
 * triggered it); return a defined value instead of crashing, per the
 * design ruling. Physical lines at or beyond the region boundary mirror
 * Remap()'s passthrough and are returned unchanged.
 */
uint64_t StartGap::Unmap( uint64_t address )
{
    uint64_t pa = address / 64, off = address % 64;
    uint64_t N = lines - 1;

    if( pa >= lines )
        return address;

    if( pa == gap )
        return ( ( gap - start + N ) % N ) * 64 + off;

    uint64_t s = ( pa < gap ) ? pa : pa - 1;
    uint64_t orig = ( s + N - start ) % N;   /* s, N, start are all < 2^63; no underflow before the mod */
    return orig * 64 + off;
}

void StartGap::Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank,
                          uint64_t *rank, uint64_t *channel, uint64_t *subarray )
{
    AddressTranslator::Translate( Remap( address ), row, col, bank, rank, channel, subarray );
}

uint64_t StartGap::ReverseTranslate( const uint64_t& row, const uint64_t& col, const uint64_t& bank,
                                     const uint64_t& rank, const uint64_t& channel, const uint64_t& subarray )
{
    return Unmap( AddressTranslator::ReverseTranslate( row, col, bank, rank, channel, subarray ) );
}

/*
 * GapMove(): one step of Qureshi Fig. 4's rotation, invoked every
 * StartGapInterval writes. GAP counts down to 0; once there, a full
 * rotation of the region has completed, so GAP resets to N and START
 * advances by one logical line for the next rotation.
 */
void StartGap::NoteWrite( )
{
    startGapWrites++;
    writesSinceMove++;

    if( writesSinceMove >= interval )
    {
        uint64_t N = lines - 1;

        if( gap > 0 )
            gap = gap - 1;
        else
        {
            gap = N;
            start = ( N > 0 ) ? ( start + 1 ) % N : 0;
        }

        writesSinceMove = 0;
        startGapMoves++;
    }
}

void StartGap::RegisterStats( )
{
    AddStat(startGapMoves);
    AddStat(startGapWrites);
    AddStat(startGapOutOfRegion);
    AddStat(startGapBoundaryAlias);
}
