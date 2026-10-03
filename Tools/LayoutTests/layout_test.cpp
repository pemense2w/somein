// Property tests for the course generator. Build + run:
//   g++ -std=c++17 -O1 -Wall -Wextra -Wshadow -I Source/GemRoller Tools/LayoutTests/layout_test.cpp -o /tmp/layout_test && /tmp/layout_test
#include "GRLayout.h"

#include <algorithm>
#include <cstdio>
#include <string>

using namespace GRLayout;

static int Failures = 0;
static int Checks = 0;

#define EXPECT(cond, ...)                                                    \
    do                                                                       \
    {                                                                        \
        ++Checks;                                                            \
        if (!(cond))                                                         \
        {                                                                    \
            if (++Failures <= 25)                                            \
            {                                                                \
                std::printf("FAIL %s:%d  %s  -- ", __FILE__, __LINE__, #cond); \
                std::printf(__VA_ARGS__);                                    \
                std::printf("\n");                                           \
            }                                                                \
        }                                                                    \
    } while (0)

static double Dist2D(const V3& A, const V3& B) { return (A - B).Length2D(); }

static void CheckCourse(uint32_t Seed, int N)
{
    const Course C = Generate(Seed, N);
    const int NI = int(C.Islands.size());
    EXPECT(NI == N, "seed %u: %d islands, wanted %d", Seed, NI, N);
    EXPECT(int(C.Links.size()) == N - 1, "seed %u: link count", Seed);

    // --- islands: no overlaps, sensible sizes, always climbing ------------------------------
    for (int I = 0; I < NI; ++I)
    {
        const Island& A = C.Islands[I];
        EXPECT(A.Radius >= 300.0 && A.Radius <= 500.0, "seed %u island %d radius %.0f", Seed, I, A.Radius);
        EXPECT(A.Radius > kSpinnerReach + 60.0, "seed %u island %d too small for its spinner", Seed, I);
        if (I > 0)
        {
            EXPECT(A.Center.Z > C.Islands[I - 1].Center.Z + 40.0, "seed %u island %d not above previous", Seed, I);
        }
        for (int J = I + 1; J < NI; ++J)
        {
            const Island& B = C.Islands[J];
            const double Sep = Dist2D(A.Center, B.Center) - A.Radius - B.Radius;
            EXPECT(Sep > 250.0 || std::fabs(A.Center.Z - B.Center.Z) > 400.0,
                   "seed %u islands %d,%d overlap (sep %.0f)", Seed, I, J, Sep);
        }
        // respawn point sits on the island, above the surface
        EXPECT(Dist2D(A.Respawn, A.Center) < A.Radius * 0.9, "seed %u respawn %d off island", Seed, I);
        EXPECT(A.Respawn.Z > A.Center.Z + kMarbleRadius, "seed %u respawn %d under surface", Seed, I);
        EXPECT(Dist2D(A.CheckpointPole, A.Center) < A.Radius, "seed %u pole %d off island", Seed, I);
        for (const V3& T : A.Trees)
        {
            EXPECT(Dist2D(T, A.Center) < A.Radius, "seed %u tree off island %d", Seed, I);
        }
    }
    EXPECT(C.Islands.back().bGoal, "seed %u: last island must be the goal", Seed);
    EXPECT(!C.Islands.front().bSpinner, "seed %u: first island must be safe", Seed);

    // --- links: every kind must be physically crossable --------------------------------------
    int PadLinks = 0, StoneLinks = 0, MoverLinks = 0;
    for (const Link& L : C.Links)
    {
        const Island& From = C.Islands[L.From];
        const Island& To = C.Islands[L.To];
        EXPECT(L.To == L.From + 1, "seed %u link order", Seed);
        switch (L.Kind)
        {
        case LinkKind::JumpPad:
        {
            ++PadLinks;
            EXPECT(Dist2D(L.PadPos, From.Center) < From.Radius * 0.9, "seed %u pad off island", Seed);
            const V3 Start(L.PadPos.X, L.PadPos.Y, L.PadPos.Z + kMarbleRadius);
            const V3 Vel = BallisticVelocity(Start, L.Landing, L.FlightTime);
            const double Hspeed = std::sqrt(Vel.X * Vel.X + Vel.Y * Vel.Y);
            EXPECT(Hspeed < 800.0, "seed %u pad horizontal speed %.0f too fast to land", Seed, Hspeed);
            const double Apex = Vel.Z * Vel.Z / (2.0 * kGravity);
            EXPECT(Apex < 900.0, "seed %u pad apex %.0f", Seed, Apex);
            // the parabola must land exactly on the landing spot
            const V3 End = BallisticPoint(Start, Vel, L.FlightTime);
            EXPECT((End - L.Landing).Length() < 1e-6, "seed %u ballistic mismatch", Seed);
            // landing zone is on the destination island with a long runway ahead of it
            EXPECT(Dist2D(L.Landing, To.Center) < To.Radius * 0.5, "seed %u landing off target island", Seed);
            // even when launched from the edge of the pad, the corrected shot still lands
            for (int K = 0; K < 8; ++K)
            {
                const double A = K * kPi / 4.0;
                const V3 Off(Start.X + std::cos(A) * 110.0, Start.Y + std::sin(A) * 110.0, Start.Z);
                const V3 V2 = BallisticVelocity(Off, L.Landing, L.FlightTime);
                EXPECT(std::sqrt(V2.X * V2.X + V2.Y * V2.Y) < 950.0, "seed %u off-centre launch too fast", Seed);
            }
            break;
        }
        case LinkKind::StepStones:
        {
            ++StoneLinks;
            EXPECT(L.Stones.size() >= 2 && L.Stones.size() <= 3, "seed %u stones %zu", Seed, L.Stones.size());
            V3 PrevPos = From.Center;
            double PrevRad = From.Radius;
            for (const V3& S : L.Stones)
            {
                const double EdgeGap = Dist2D(S, PrevPos) - PrevRad - kStoneRadius;
                EXPECT(EdgeGap > 90.0 && EdgeGap < 170.0, "seed %u stone edge gap %.0f", Seed, EdgeGap);
                EXPECT(S.Z - PrevPos.Z < 100.0 && S.Z - PrevPos.Z > 0.0, "seed %u stone step %.0f", Seed, S.Z - PrevPos.Z);
                PrevPos = S;
                PrevRad = kStoneRadius;
            }
            const double Last = Dist2D(To.Center, PrevPos) - To.Radius - kStoneRadius;
            EXPECT(Last > 90.0 && Last < 170.0, "seed %u last stone gap %.0f", Seed, Last);
            EXPECT(To.Center.Z - PrevPos.Z < 100.0, "seed %u last stone step %.0f", Seed, To.Center.Z - PrevPos.Z);
            break;
        }
        case LinkKind::MovingPlatform:
        {
            ++MoverLinks;
            const double Pr = kStoneRadius * kMoverScale;
            const double GapA = Dist2D(L.MoverA, From.Center) - From.Radius - Pr;
            const double GapB = Dist2D(L.MoverB, To.Center) - To.Radius - Pr;
            EXPECT(std::fabs(GapA - 45.0) < 1.0 && std::fabs(GapB - 45.0) < 1.0, "seed %u mover gaps %.1f %.1f", Seed, GapA, GapB);
            EXPECT(std::fabs(L.MoverA.Z - From.Center.Z) < 1e-6 && std::fabs(L.MoverB.Z - To.Center.Z) < 1e-6, "seed %u mover heights", Seed);
            const double Len = Dist2D(L.MoverA, L.MoverB);
            EXPECT(Len > 250.0 && Len < 650.0, "seed %u mover travel %.0f", Seed, Len);
            EXPECT(L.MoverPeriod >= 4.4 && L.MoverPeriod <= 6.6, "seed %u mover period", Seed);
            break;
        }
        }
    }
    EXPECT(C.Links.front().Kind == LinkKind::JumpPad, "seed %u: first link is the gem-guided jump", Seed);
    EXPECT(PadLinks + StoneLinks + MoverLinks == N - 1, "seed %u link kinds", Seed);
    for (size_t I = 2; I < C.Links.size(); ++I)
    {
        const bool Three = C.Links[I].Kind == C.Links[I - 1].Kind && C.Links[I].Kind == C.Links[I - 2].Kind;
        EXPECT(!Three, "seed %u: three identical links in a row at %zu", Seed, I);
    }

    // --- pickups ---------------------------------------------------------------------------
    EXPECT(C.Gems.size() >= size_t(8 * N) && C.Gems.size() <= size_t(16 * N), "seed %u gem count %zu for %d islands", Seed, C.Gems.size(), N);
    int Golden = 0;
    for (const GemSpot& G : C.Gems)
    {
        Golden += G.bGolden ? 1 : 0;
        EXPECT(G.Pos.Z > C.KillZ + 1500.0, "seed %u gem below the world", Seed);
    }
    EXPECT(Golden >= 1, "seed %u: no golden gem", Seed);
    EXPECT(!C.TimeCrystals.empty() || N < 4, "seed %u: no time crystals", Seed);

    // gems must never overlap each other (they would be collected as one)
    for (size_t I = 0; I < C.Gems.size(); ++I)
    {
        for (size_t J = I + 1; J < C.Gems.size(); ++J)
        {
            EXPECT((C.Gems[I].Pos - C.Gems[J].Pos).Length() > 40.0, "seed %u gems %zu,%zu overlap", Seed, I, J);
        }
    }

    // --- start, goal, world -----------------------------------------------------------------
    EXPECT((C.Start - C.Islands.front().Respawn).Length() < 1e-6, "seed %u start", Seed);
    EXPECT(Dist2D(C.GoalPos, C.Islands.back().Center) < C.Islands.back().Radius * 0.4, "seed %u goal off island", Seed);
    EXPECT(std::fabs(C.GoalPos.Z - (C.Islands.back().Center.Z + kGoalRingLift)) < 1e-6, "seed %u goal height", Seed);
    EXPECT(C.KillZ < C.Islands.front().Center.Z - 1500.0, "seed %u kill plane too high", Seed);
    EXPECT(std::fabs(C.TimeBudget - (30.0 + 11.0 * (N - 1))) < 1e-9, "seed %u time budget %.0f", Seed, C.TimeBudget);
    EXPECT(C.Rocks.size() >= 10, "seed %u only %zu backdrop rocks", Seed, C.Rocks.size());
    for (const BackdropRock& Rk : C.Rocks)
    {
        for (const Island& Isl : C.Islands)
        {
            EXPECT((Rk.Pos - Isl.Center).Length() > 2200.0, "seed %u backdrop rock too close to an island", Seed);
            EXPECT(Rk.Scale < 2.0, "seed %u backdrop rock bigger than the play islands", Seed);
        }
    }
    EXPECT(C.Clouds.size() == 14, "seed %u clouds", Seed);
    for (const CloudSpot& Cl : C.Clouds)
    {
        EXPECT(Cl.Pos.Z < C.Islands.front().Center.Z - 700.0, "seed %u cloud in the play space", Seed);
    }
}

int main()
{
    int Total = 0;
    long PadN = 0, StoneN = 0, MoverN = 0, SpinN = 0, BoostN = 0, GemN = 0;
    for (uint32_t Seed = 1; Seed <= 3000; ++Seed)
    {
        CheckCourse(Seed, 9);
        ++Total;
    }
    for (uint32_t Seed = 1; Seed <= 400; ++Seed)
    {
        CheckCourse(Seed * 7919u, 5 + int(Seed % 9));  // other course lengths
        ++Total;
    }
    for (uint32_t Seed = 1; Seed <= 300; ++Seed)
    {
        const Course C = Generate(Seed, 9);
        for (const Link& L : C.Links)
        {
            PadN += L.Kind == LinkKind::JumpPad;
            StoneN += L.Kind == LinkKind::StepStones;
            MoverN += L.Kind == LinkKind::MovingPlatform;
        }
        for (const Island& I : C.Islands)
        {
            SpinN += I.bSpinner;
        }
        BoostN += long(C.BoostPads.size());
        GemN += long(C.Gems.size());
    }

    // determinism: the same seed must always give the same course
    const Course A = Generate(12345, 9);
    const Course B = Generate(12345, 9);
    bool Same = A.Gems.size() == B.Gems.size() && A.Islands.size() == B.Islands.size();
    for (size_t I = 0; Same && I < A.Gems.size(); ++I)
    {
        Same = (A.Gems[I].Pos - B.Gems[I].Pos).Length() == 0.0;
    }
    EXPECT(Same, "generation is not deterministic");
    const Course D = Generate(12346, 9);
    EXPECT((D.Islands[3].Center - A.Islands[3].Center).Length() > 1.0, "different seeds gave identical courses");

    std::printf("%d courses, %d checks, %d failures\n", Total + 2, Checks, Failures);
    std::printf("mix over 300 seeds: jump pads %ld, step stones %ld, movers %ld | spinners %ld, boost pads %ld, avg gems %.1f\n",
                PadN, StoneN, MoverN, SpinN, BoostN, double(GemN) / 300.0);
    return Failures == 0 ? 0 : 1;
}
