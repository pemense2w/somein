// Prints a generated course as JSON so Blender can render a faithful preview of it.
//   g++ -std=c++17 -I Source/GemRoller Tools/LayoutTests/layout_dump.cpp -o /tmp/layout_dump
//   /tmp/layout_dump 2024 > /tmp/course.json
#include "GRLayout.h"

#include <cstdio>
#include <cstdlib>

using namespace GRLayout;

static void Vec(const V3& V) { std::printf("[%.1f,%.1f,%.1f]", V.X, V.Y, V.Z); }

int main(int Argc, char** Argv)
{
    const uint32_t Seed = Argc > 1 ? uint32_t(std::strtoul(Argv[1], nullptr, 10)) : 1u;
    const Course C = Generate(Seed, 9);
    std::printf("{\"seed\":%u,\"timeBudget\":%.1f,\"killZ\":%.1f,\n", C.Seed, C.TimeBudget, C.KillZ);
    std::printf("\"start\":");
    Vec(C.Start);
    std::printf(",\"goal\":");
    Vec(C.GoalPos);
    std::printf(",\"goalYaw\":%.2f,\n\"islands\":[", C.GoalYaw);
    for (size_t I = 0; I < C.Islands.size(); ++I)
    {
        const Island& Isl = C.Islands[I];
        std::printf("%s{\"c\":", I ? "," : "");
        Vec(Isl.Center);
        std::printf(",\"scale\":%.3f,\"yaw\":%.1f,\"rock\":%d,\"spinner\":%s,\"spin\":%.1f,\"goal\":%s,\"pole\":", Isl.Scale, Isl.Yaw,
                    Isl.RockVariant, Isl.bSpinner ? "true" : "false", Isl.SpinRate, Isl.bGoal ? "true" : "false");
        Vec(Isl.CheckpointPole);
        std::printf(",\"trees\":[");
        for (size_t T = 0; T < Isl.Trees.size(); ++T)
        {
            std::printf("%s", T ? "," : "");
            Vec(Isl.Trees[T]);
        }
        std::printf("]}");
    }
    std::printf("],\n\"links\":[");
    for (size_t I = 0; I < C.Links.size(); ++I)
    {
        const Link& L = C.Links[I];
        std::printf("%s{\"kind\":\"%s\"", I ? "," : "",
                    L.Kind == LinkKind::JumpPad ? "pad" : (L.Kind == LinkKind::StepStones ? "stones" : "mover"));
        std::printf(",\"pad\":");
        Vec(L.PadPos);
        std::printf(",\"landing\":");
        Vec(L.Landing);
        std::printf(",\"t\":%.3f,\"stones\":[", L.FlightTime);
        for (size_t S = 0; S < L.Stones.size(); ++S)
        {
            std::printf("%s", S ? "," : "");
            Vec(L.Stones[S]);
        }
        std::printf("],\"a\":");
        Vec(L.MoverA);
        std::printf(",\"b\":");
        Vec(L.MoverB);
        std::printf("}");
    }
    std::printf("],\n\"gems\":[");
    for (size_t I = 0; I < C.Gems.size(); ++I)
    {
        std::printf("%s{\"p\":", I ? "," : "");
        Vec(C.Gems[I].Pos);
        std::printf(",\"gold\":%s}", C.Gems[I].bGolden ? "true" : "false");
    }
    std::printf("],\n\"crystals\":[");
    for (size_t I = 0; I < C.TimeCrystals.size(); ++I)
    {
        std::printf("%s", I ? "," : "");
        Vec(C.TimeCrystals[I]);
    }
    std::printf("],\n\"boost\":[");
    for (size_t I = 0; I < C.BoostPads.size(); ++I)
    {
        std::printf("%s{\"p\":", I ? "," : "");
        Vec(C.BoostPads[I].Pos);
        std::printf(",\"yaw\":%.1f}", C.BoostPads[I].Yaw);
    }
    std::printf("],\n\"rocks\":[");
    for (size_t I = 0; I < C.Rocks.size(); ++I)
    {
        std::printf("%s{\"p\":", I ? "," : "");
        Vec(C.Rocks[I].Pos);
        std::printf(",\"s\":%.2f,\"yaw\":%.1f,\"v\":%d}", C.Rocks[I].Scale, C.Rocks[I].Yaw, C.Rocks[I].Variant);
    }
    std::printf("],\n\"clouds\":[");
    for (size_t I = 0; I < C.Clouds.size(); ++I)
    {
        std::printf("%s{\"p\":", I ? "," : "");
        Vec(C.Clouds[I].Pos);
        std::printf(",\"s\":%.2f,\"yaw\":%.1f,\"v\":%d}", C.Clouds[I].Scale, C.Clouds[I].Yaw, C.Clouds[I].Variant);
    }
    std::printf("]}\n");
    return 0;
}
