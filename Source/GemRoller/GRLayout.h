// Gem Roller - course layout generator.
//
// Plain C++ on purpose: no Unreal types, so the exact same code is compiled by the
// unit tests in Tools/LayoutTests (g++ / clang) and by the game. It turns a seed into a
// list of islands, links between them, pickups and scenery, all in Unreal units
// (centimetres, Z up). The game mode just spawns whatever this returns.
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace GRLayout
{
constexpr double kPi            = 3.14159265358979323846;
constexpr double kGravity       = 1500.0;  // UU/s^2, must match PhysicsSettings.DefaultGravityZ
constexpr double kMarbleRadius  = 50.0;    // SM_Marble
constexpr double kIslandRadius  = 350.0;   // SM_Island at scale 1
constexpr double kStoneRadius   = 120.0;   // SM_StepStone at scale 1
constexpr double kSpinnerReach  = 220.0;   // SM_Spinner arm half length
constexpr double kMoverScale    = 1.8;     // SM_StepStone scale used for moving platforms
constexpr double kGemHeight     = 85.0;    // gem centre above the surface it sits on
constexpr double kGoalRingLift  = 190.0;   // goal ring centre above the surface

struct V3
{
    double X = 0.0, Y = 0.0, Z = 0.0;
    V3() = default;
    V3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
    V3 operator+(const V3& O) const { return V3(X + O.X, Y + O.Y, Z + O.Z); }
    V3 operator-(const V3& O) const { return V3(X - O.X, Y - O.Y, Z - O.Z); }
    V3 operator*(double S) const { return V3(X * S, Y * S, Z * S); }
    double Length2D() const { return std::sqrt(X * X + Y * Y); }
    double Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }
};

// Deterministic across platforms (SplitMix64), so a seed always makes the same course.
class Rng
{
public:
    explicit Rng(uint32_t Seed) : State(0x9E3779B97F4A7C15ull ^ (uint64_t(Seed) * 0xD1B54A32D192ED03ull)) {}
    uint64_t Next()
    {
        State += 0x9E3779B97F4A7C15ull;
        uint64_t Z = State;
        Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
        Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
        return Z ^ (Z >> 31);
    }
    double Unit() { return double(Next() >> 11) * (1.0 / 9007199254740992.0); }
    double Range(double Lo, double Hi) { return Lo + (Hi - Lo) * Unit(); }
    int Int(int Lo, int HiInclusive) { return Lo + int(Unit() * double(HiInclusive - Lo + 1)); }
    bool Chance(double P) { return Unit() < P; }

private:
    uint64_t State;
};

enum class LinkKind
{
    JumpPad,
    StepStones,
    MovingPlatform
};

struct Island
{
    V3 Center;           // centre of the walkable top surface
    double Scale = 1.0;  // uniform scale of SM_Island
    double Radius = 350.0;
    double Yaw = 0.0;    // degrees, cosmetic
    int RockVariant = 0; // SM_Rock_A/B/C hangs underneath
    bool bSpinner = false;
    double SpinRate = 0.0;  // deg/s, signed
    bool bGoal = false;
    V3 CheckpointPole;      // where the flag stands
    V3 Respawn;             // where the marble comes back after a fall (ball centre)
    double RespawnYaw = 0.0; // degrees, faces the way forward
    std::vector<V3> Trees;
};

struct Link
{
    int From = 0;
    int To = 0;
    LinkKind Kind = LinkKind::JumpPad;

    // JumpPad: the pad launches the marble on a parabola that ends at Landing in FlightTime seconds.
    V3 PadPos;
    V3 Landing;
    double FlightTime = 1.2;

    // StepStones: top-surface centres, in order.
    std::vector<V3> Stones;

    // MovingPlatform: oscillates A <-> B (top-surface centres).
    V3 MoverA, MoverB;
    double MoverPeriod = 5.0;
    double MoverPhase = 0.0;
};

struct GemSpot
{
    V3 Pos;
    bool bGolden = false;
};

struct BoostPadSpot
{
    V3 Pos;
    double Yaw = 0.0; // degrees, direction of boost
};

struct BackdropRock
{
    V3 Pos;
    double Scale = 1.0;
    double Yaw = 0.0;
    int Variant = 0;
    double BobPhase = 0.0;
};

struct CloudSpot
{
    V3 Pos;
    double Scale = 1.0;
    double Yaw = 0.0;
    int Variant = 0;
};

struct Course
{
    uint32_t Seed = 0;
    std::vector<Island> Islands;
    std::vector<Link> Links;
    std::vector<GemSpot> Gems;
    std::vector<V3> TimeCrystals;
    std::vector<BoostPadSpot> BoostPads;
    std::vector<BackdropRock> Rocks;
    std::vector<CloudSpot> Clouds;
    V3 Start;               // ball centre
    double StartYaw = 0.0;  // degrees
    V3 GoalPos;             // centre of the goal ring
    double GoalYaw = 0.0;   // degrees, direction of travel through the ring
    double KillZ = -2000.0;
    double TimeBudget = 120.0;
};

// Velocity that takes a ball from `From` to `To` in `T` seconds under gravity.
inline V3 BallisticVelocity(const V3& From, const V3& To, double T)
{
    const V3 D = To - From;
    return V3(D.X / T, D.Y / T, D.Z / T + 0.5 * kGravity * T);
}

// Ball position `T` seconds after leaving `From` with velocity `Vel`.
inline V3 BallisticPoint(const V3& From, const V3& Vel, double T)
{
    return V3(From.X + Vel.X * T, From.Y + Vel.Y * T, From.Z + Vel.Z * T - 0.5 * kGravity * T * T);
}

namespace Detail
{
constexpr double DegToRad = kPi / 180.0;

inline V3 Dir(double Rad) { return V3(std::cos(Rad), std::sin(Rad), 0.0); }
inline V3 Perp(const V3& D) { return V3(-D.Y, D.X, 0.0); }
inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

inline LinkKind PickKind(Rng& R, int LinkIndex, const std::vector<Link>& Done)
{
    if (LinkIndex == 0)
    {
        return LinkKind::JumpPad;  // friendly first jump: follow the gems
    }
    const double U = R.Unit();
    LinkKind Choice = U < 0.36 ? LinkKind::JumpPad : (U < 0.70 ? LinkKind::StepStones : LinkKind::MovingPlatform);

    // Never three of the same kind in a row: if that is what we rolled, switch to one of the other two.
    const int N = int(Done.size());
    if (N >= 2 && Done[N - 1].Kind == Done[N - 2].Kind && Choice == Done[N - 1].Kind)
    {
        Choice = static_cast<LinkKind>((static_cast<int>(Choice) + 1 + R.Int(0, 1)) % 3);
    }
    return Choice;
}

inline void AddIslandGems(Course& C, const Island& Isl, const V3& ExitDir, Rng& R)
{
    const double Z = Isl.Center.Z + kGemHeight;
    const V3 Perp2 = Perp(ExitDir);
    const double Rad = Isl.Radius;

    int Pattern = R.Int(0, 3);
    if (Pattern == 1 && Isl.bSpinner)
    {
        Pattern = 0;  // a straight line would run into the hub
    }

    switch (Pattern)
    {
    case 0:  // ring
        for (int K = 0; K < 8; ++K)
        {
            const double A = 2.0 * kPi * K / 8.0;
            C.Gems.push_back({V3(Isl.Center.X + std::cos(A) * Rad * 0.62, Isl.Center.Y + std::sin(A) * Rad * 0.62, Z), false});
        }
        break;
    case 1:  // line through the middle, entry to exit
        for (int K = -2; K <= 2; ++K)
        {
            const V3 P = Isl.Center + ExitDir * (K * Rad * 0.30);
            C.Gems.push_back({V3(P.X, P.Y, Z), false});
        }
        break;
    case 2:  // arc on the exit side
        for (int K = 0; K < 6; ++K)
        {
            const double A = -0.55 * kPi + 1.1 * kPi * K / 5.0;
            const V3 P = Isl.Center + ExitDir * (std::cos(A) * Rad * 0.66) + Perp2 * (std::sin(A) * Rad * 0.66);
            C.Gems.push_back({V3(P.X, P.Y, Z), false});
        }
        break;
    default:  // zig-zag
        for (int K = -2; K <= 2; ++K)
        {
            const V3 P = Isl.Center + ExitDir * (K * Rad * 0.28) + Perp2 * ((K % 2 == 0 ? 1.0 : -1.0) * Rad * 0.30);
            C.Gems.push_back({V3(P.X, P.Y, Z), false});
        }
        break;
    }

    if (Isl.bSpinner)
    {
        C.Gems.push_back({V3(Isl.Center.X, Isl.Center.Y, Isl.Center.Z + 150.0), true});  // above the hub: jump for it
    }
    else if (!Isl.bGoal && R.Chance(0.4))
    {
        const V3 P = Isl.Center + Perp2 * (Rad * 0.86);  // out on the rim: greedy
        C.Gems.push_back({V3(P.X, P.Y, Z), true});
    }
}
}  // namespace Detail

inline Course Generate(uint32_t Seed, int NumIslands = 9)
{
    using namespace Detail;
    if (NumIslands < 3)
    {
        NumIslands = 3;
    }

    Course C;
    C.Seed = Seed;
    Rng R(Seed);

    double Heading = 0.0;  // radians; +X is "forward" at the start
    double TurnSum = 0.0;
    std::vector<V3> InDir;  // direction of travel into each island
    std::vector<V3> OutDir; // direction of travel out of each island

    Island First;
    First.Scale = 1.1;
    First.Radius = kIslandRadius * First.Scale;
    First.Yaw = R.Range(0.0, 360.0);
    First.RockVariant = R.Int(0, 2);
    C.Islands.push_back(First);
    InDir.push_back(Dir(0.0));

    for (int I = 1; I < NumIslands; ++I)
    {
        const bool bLast = (I == NumIslands - 1);
        Link L;
        L.From = I - 1;
        L.To = I;
        L.Kind = PickKind(R, I - 1, C.Links);

        double Turn = R.Range(-40.0, 40.0) * DegToRad;
        if (std::fabs(TurnSum + Turn) > 80.0 * DegToRad)
        {
            Turn = -Turn;
        }
        TurnSum += Turn;
        Heading += Turn;
        const V3 D = Dir(Heading);
        OutDir.push_back(D);
        InDir.push_back(D);

        Island Isl;
        Isl.Scale = bLast ? 1.4 : R.Range(0.9, 1.2);
        Isl.Radius = kIslandRadius * Isl.Scale;
        Isl.Yaw = R.Range(0.0, 360.0);
        Isl.RockVariant = R.Int(0, 2);
        Isl.bGoal = bLast;
        Isl.bSpinner = !bLast && I >= 2 && R.Chance(0.55);
        Isl.SpinRate = (R.Chance(0.5) ? 1.0 : -1.0) * R.Range(55.0, 95.0);

        const Island& Prev = C.Islands.back();
        double Gap = 0.0;
        double Dz = 0.0;
        double StoneGap = 0.0;
        int StoneCount = 0;
        switch (L.Kind)
        {
        case LinkKind::JumpPad:
            Gap = R.Range(600.0, 950.0);
            Dz = R.Range(100.0, 280.0);
            break;
        case LinkKind::StepStones:
            StoneCount = R.Int(2, 3);
            StoneGap = R.Range(100.0, 160.0);
            Gap = (StoneCount + 1) * StoneGap + 2.0 * StoneCount * kStoneRadius;
            Dz = R.Range(60.0, 100.0 * (StoneCount + 1) * 0.8);
            break;
        case LinkKind::MovingPlatform:
            Gap = R.Range(850.0, 1100.0);
            Dz = R.Range(60.0, 200.0);
            break;
        }

        Isl.Center = Prev.Center + D * (Prev.Radius + Gap + Isl.Radius);
        Isl.Center.Z = Prev.Center.Z + Dz;

        // --- link geometry -------------------------------------------------------------
        switch (L.Kind)
        {
        case LinkKind::JumpPad:
        {
            const double PadFrac = Prev.bSpinner ? 0.80 : 0.68;
            L.PadPos = Prev.Center + D * (Prev.Radius * PadFrac);
            L.PadPos.Z = Prev.Center.Z;
            L.Landing = Isl.Center - D * (Isl.Radius * 0.30);
            L.Landing.Z = Isl.Center.Z + kMarbleRadius + 8.0;
            const V3 Start = V3(L.PadPos.X, L.PadPos.Y, L.PadPos.Z + kMarbleRadius);
            const double Dxy = (L.Landing - Start).Length2D();
            L.FlightTime = Clamp(Dxy / 650.0, 1.15, 1.9);
            const V3 Vel = BallisticVelocity(Start, L.Landing, L.FlightTime);
            for (int K = 1; K <= 5; ++K)
            {
                const double T = L.FlightTime * (0.15 + 0.14 * K);
                C.Gems.push_back({BallisticPoint(Start, Vel, T), false});
            }
            break;
        }
        case LinkKind::StepStones:
            for (int K = 0; K < StoneCount; ++K)
            {
                const double S = Prev.Radius + StoneGap + kStoneRadius + K * (2.0 * kStoneRadius + StoneGap);
                V3 P = Prev.Center + D * S;
                P.Z = Prev.Center.Z + Dz * (K + 1) / double(StoneCount + 1);
                L.Stones.push_back(P);
                C.Gems.push_back({V3(P.X, P.Y, P.Z + kGemHeight + 15.0), false});
            }
            break;
        case LinkKind::MovingPlatform:
        {
            const double Pr = kStoneRadius * kMoverScale;
            const double Margin = 45.0;
            L.MoverA = Prev.Center + D * (Prev.Radius + Margin + Pr);
            L.MoverA.Z = Prev.Center.Z;
            L.MoverB = Isl.Center - D * (Isl.Radius + Margin + Pr);
            L.MoverB.Z = Isl.Center.Z;
            L.MoverPeriod = R.Range(4.5, 6.5);
            L.MoverPhase = R.Range(0.0, 2.0 * kPi);
            for (int K = 1; K <= 3; ++K)
            {
                const double T = K / 4.0;
                const V3 P = L.MoverA + (L.MoverB - L.MoverA) * T;
                C.Gems.push_back({V3(P.X, P.Y, P.Z + kGemHeight + 40.0), false});
            }
            break;
        }
        }

        // --- island dressing -----------------------------------------------------------
        const V3 In = InDir.back();
        Isl.CheckpointPole = Isl.Center - In * (Isl.Radius * 0.72) + Perp(In) * (Isl.Radius * 0.38);
        Isl.CheckpointPole.Z = Isl.Center.Z;
        Isl.Respawn = Isl.Center - In * (Isl.Radius * 0.45);
        Isl.Respawn.Z = Isl.Center.Z + kMarbleRadius + 20.0;
        Isl.RespawnYaw = std::atan2(In.Y, In.X) / DegToRad;

        C.Islands.push_back(Isl);
        C.Links.push_back(L);
    }

    // Exit direction of the final island = the way we were already heading.
    OutDir.push_back(OutDir.empty() ? Dir(0.0) : OutDir.back());

    // First island: start near the back edge, facing forward.
    {
        Island& F = C.Islands.front();
        const V3 Out = OutDir.front();
        F.CheckpointPole = F.Center - Out * (F.Radius * 0.62) + Perp(Out) * (F.Radius * 0.45);
        F.Respawn = F.Center - Out * (F.Radius * 0.40);
        F.Respawn.Z = F.Center.Z + kMarbleRadius + 20.0;
        F.RespawnYaw = std::atan2(Out.Y, Out.X) / DegToRad;
        C.Start = F.Respawn;
        C.StartYaw = F.RespawnYaw;
    }

    // Per-island pickups, trees, boost pads, goal.
    for (int I = 0; I < NumIslands; ++I)
    {
        Island& Isl = C.Islands[I];
        const V3 Out = OutDir[I];
        const V3 In = InDir[I];

        // Trees on the rim, away from the lanes the marble uses.
        const int TreeCount = R.Int(2, 4);
        const double InAngle = std::atan2(In.Y, In.X);
        const double OutAngle = std::atan2(Out.Y, Out.X);
        for (int T = 0, Tries = 0; T < TreeCount && Tries < 30; ++Tries)
        {
            const double A = R.Range(0.0, 2.0 * kPi);
            const auto AngDiff = [](double P, double Q)
            {
                double Dv = std::fmod(P - Q + 3.0 * kPi, 2.0 * kPi) - kPi;
                return std::fabs(Dv);
            };
            if (AngDiff(A, InAngle + kPi) < 0.55 || AngDiff(A, OutAngle) < 0.55)
            {
                continue;
            }
            const double Rr = Isl.Radius * R.Range(0.86, 0.93);
            Isl.Trees.push_back(V3(Isl.Center.X + std::cos(A) * Rr, Isl.Center.Y + std::sin(A) * Rr, Isl.Center.Z));
            ++T;
        }

        AddIslandGems(C, Isl, Out, R);

        // Boost pad on the entry side, pointing at the exit; never under a spinner arm.
        if (I >= 1 && !Isl.bGoal && !Isl.bSpinner && R.Chance(0.45))
        {
            BoostPadSpot B;
            B.Pos = Isl.Center - Out * (Isl.Radius * 0.50);
            B.Pos.Z = Isl.Center.Z + 2.0;
            B.Yaw = std::atan2(Out.Y, Out.X) / DegToRad;
            C.BoostPads.push_back(B);
        }

        // Time crystals on a couple of mid-course islands.
        if (!Isl.bGoal && (I == 3 || I == 6))
        {
            const V3 P = Isl.Center + Perp(Out) * (Isl.Radius * 0.45);
            C.TimeCrystals.push_back(V3(P.X, P.Y, Isl.Center.Z + 95.0));
        }
    }

    // Goal ring stands on the last island, facing the way we came in.
    {
        const Island& G = C.Islands.back();
        const V3 In = InDir.back();
        C.GoalPos = G.Center + In * (G.Radius * 0.15);
        C.GoalPos.Z = G.Center.Z + kGoalRingLift;
        C.GoalYaw = std::atan2(In.Y, In.X) / DegToRad;
    }

    // --- scenery -------------------------------------------------------------------------
    double MinZ = 1e9, MaxZ = -1e9, MinX = 1e9, MaxX = -1e9, MinY = 1e9, MaxY = -1e9;
    for (const Island& Isl : C.Islands)
    {
        MinZ = std::fmin(MinZ, Isl.Center.Z);
        MaxZ = std::fmax(MaxZ, Isl.Center.Z);
        MinX = std::fmin(MinX, Isl.Center.X);
        MaxX = std::fmax(MaxX, Isl.Center.X);
        MinY = std::fmin(MinY, Isl.Center.Y);
        MaxY = std::fmax(MaxY, Isl.Center.Y);
    }
    C.KillZ = MinZ - 2200.0;

    const auto FarFromIslands = [&C](const V3& P, double Clearance)
    {
        for (const Island& Isl : C.Islands)
        {
            const double Dx = P.X - Isl.Center.X;
            const double Dy = P.Y - Isl.Center.Y;
            const double Dz2 = P.Z - Isl.Center.Z;
            if (std::sqrt(Dx * Dx + Dy * Dy + Dz2 * Dz2) < Clearance)
            {
                return false;
            }
        }
        return true;
    };

    for (int Tries = 0; Tries < 600 && int(C.Rocks.size()) < 16; ++Tries)
    {
        BackdropRock Rk;
        Rk.Scale = R.Range(0.6, 1.9);  // smaller than the play islands: they are scenery, not targets
        Rk.Pos = V3(R.Range(MinX - 5000.0, MaxX + 5000.0), R.Range(MinY - 5000.0, MaxY + 5000.0),
                    R.Range(MinZ - 1500.0, MaxZ + 900.0));
        if (!FarFromIslands(Rk.Pos, 2200.0 + 350.0 * Rk.Scale))
        {
            continue;
        }
        Rk.Yaw = R.Range(0.0, 360.0);
        Rk.Variant = R.Int(0, 2);
        Rk.BobPhase = R.Range(0.0, 2.0 * kPi);
        C.Rocks.push_back(Rk);
    }

    for (int K = 0; K < 14; ++K)
    {
        CloudSpot Cl;
        Cl.Pos = V3(R.Range(MinX - 6000.0, MaxX + 6000.0), R.Range(MinY - 6000.0, MaxY + 6000.0),
                    R.Range(MinZ - 1900.0, MinZ - 800.0));
        Cl.Scale = R.Range(0.8, 1.7);
        Cl.Yaw = R.Range(0.0, 360.0);
        Cl.Variant = R.Int(0, 1);
        C.Clouds.push_back(Cl);
    }

    C.TimeBudget = 30.0 + 11.0 * (NumIslands - 1);
    return C;
}

}  // namespace GRLayout
