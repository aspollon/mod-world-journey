/*
 * mod-world-journey - the whole world, Outland and Northrend included, as one journey from 1 to 60.
 * Released under GNU AGPL v3: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 *
 * The one place the folder name of the module matters: the core calls Add<folder>Scripts, with the dashes of the
 * folder name as underscores. Renaming the module means renaming this file and this function, nothing else.
 */

void AddSC_world_journey();

void Addmod_world_journeyScripts()
{
    AddSC_world_journey();
}
