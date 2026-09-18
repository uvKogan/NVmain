/*******************************************************************************
* MBMM T1.4: Start-Gap wear-leveling decoder for NVMain 2.0.
*
* Faithful re-implementation of Qureshi et al., "Enhancing Lifetime and
* Security of PCM-Based Main Memory with Start-Gap Wear Leveling",
* MICRO 2009, Fig. 4. A region of N logical (data) lines is mapped onto
* N+1 physical lines; the extra physical line is always empty (the
* "gap"). Every StartGapInterval writes, the gap slot rotates by one
* physical line; after N such rotations the START register advances by
* one logical line and the gap resets, so every logical line eventually
* visits every physical line once per full N+1 rotations.
*
* Gap-move data traffic (the read+write NVMain would need to physically
* relocate the line the gap steps over) is NOT modeled here; only
* startGapMoves is counted. See simulators/nvmain/CLAUDE.md PATCH LOG for
* the full rationale and the T5.2 follow-up that would model that traffic.
*
* Known limitation, accepted with visibility (review ruling, 2026-09-18):
* the region's boundary line (logical LA == N, the one line just past the
* N data lines) passes through Remap() unchanged to physical line N --
* which is also where some real data line legitimately lands whenever
* GAP < N, since Unmap() cannot tell the two sources apart from the bare
* address alone. With the default whole-channel region this boundary line
* is the channel's very last physical line, which our traces never touch,
* so this is accepted rather than redesigned. Every occurrence of this
* aliasing is counted separately in startGapBoundaryAlias (on top of the
* general startGapOutOfRegion passthrough count); a nonzero value on a
* given run flags that run's wear numbers as touching the aliased line and
* is worth a second look before trusting them.
*
* Stat semantics: startGapMoves and startGapWrites are exact per-accepted-
* write counts (NoteWrite() fires exactly once per WRITE/WRITE_PRECHARGE
* that FRFCFS::IssueCommand actually enqueues). startGapOutOfRegion is
* NOT a per-request count -- NVMain::IsIssuable() calls Translate() (hence
* Remap()) on every cycle a request is polled for admission, before it is
* finally enqueued, so this stat is informational only, inflated by
* repeated admission-poll decodes rather than by repeated writes.
*******************************************************************************/
#ifndef __DECODERS_STARTGAP_H__
#define __DECODERS_STARTGAP_H__
#include "src/AddressTranslator.h"
#include "src/Config.h"
namespace NVM {
class StartGap : public AddressTranslator
{
  public:
    StartGap( );
    ~StartGap( );
    void SetConfig( Config *config, bool createChildren = true );
    using AddressTranslator::Translate;
    virtual void Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank,
                            uint64_t *rank, uint64_t *channel, uint64_t *subarray );
    virtual uint64_t ReverseTranslate( const uint64_t& row, const uint64_t& col, const uint64_t& bank,
                                       const uint64_t& rank, const uint64_t& channel, const uint64_t& subarray );
    void RegisterStats( );
    static void NoteWrite( );            // called once per WRITE request that enters the channel

  private:
    /* Shared across all instances: the factory creates a decoder at six
     * hierarchy levels (channel, on-chip bus, off-chip bus, rank, bank,
     * plus the channel-level one made directly by MemoryController), and
     * there is exactly one Start-Gap state machine for the whole region. */
    static uint64_t start;               // START register, 0..N-1
    static uint64_t gap;                 // GAP register, 0..N (N == no pending gap-at-N reset)
    static uint64_t writesSinceMove;
    static uint64_t lines;               // physical lines in the region == N + 1 (0 until configured)
    static uint64_t interval;            // StartGapInterval, writes per gap move
    static ncounter_t startGapMoves;
    static ncounter_t startGapWrites;
    static ncounter_t startGapOutOfRegion;
    static ncounter_t startGapBoundaryAlias; // subset of startGapOutOfRegion where LA == N exactly

    uint64_t Remap( uint64_t address );
    uint64_t Unmap( uint64_t address );
};
};
#endif
