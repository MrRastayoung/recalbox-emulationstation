#include "scrapers/ScreenscraperScraper.h"
#include "Log.h"
#include "pugixml/pugixml.hpp"
#include "MetaData.h"
#include "Settings.h"
#include "Util.h"
#include <boost/assign.hpp>
#include "RecalboxConf.h"

using namespace PlatformIds;
const std::map<PlatformId, const char*> gamesdb_platformid_map = boost::assign::map_list_of
	(THREEDO, "3DO")
	(AMIGA, "Amiga")
	(AMSTRAD_CPC, "Amstrad CPC")
	(APPLE_II, "Apple II")
	(ARCADE, "Arcade")
	(ATARI_800, "Atari 800")
	(ATARI_2600, "Atari 2600")
	(ATARI_5200, "Atari 5200")
	(ATARI_7800, "Atari 7800")
	(ATARI_LYNX, "Atari Lynx")
	(ATARI_ST, "Atari ST")
	(ATARI_JAGUAR, "Atari Jaguar")
	(ATARI_JAGUAR_CD, "Atari Jaguar CD")
	(ATARI_XE, "Atari XE")
	(COLECOVISION, "Colecovision")
	(COMMODORE_64, "Commodore 64")
	(FAMILY_COMPUTER_DISK_SYSTEM, "Famicom Disk System")
	(INTELLIVISION, "Intellivision")
	(MACOS, "Mac OS")
	(XBOX, "Microsoft Xbox")
	(XBOX_360, "Microsoft Xbox 360")
	(MSX, "MSX")
	(NEOGEO, "NeoGeo")
	(NEOGEO_POCKET, "Neo Geo Pocket")
	(NEOGEO_POCKET_COLOR, "Neo Geo Pocket Color")
	(NINTENDO_3DS, "Nintendo 3DS")
	(NINTENDO_64, "Nintendo 64")
	(NINTENDO_DS, "Nintendo DS")
	(NINTENDO_ENTERTAINMENT_SYSTEM, "Nintendo Entertainment System (NES)")
	(GAME_BOY, "Nintendo Game Boy")
	(GAME_BOY_ADVANCE, "Nintendo Game Boy Advance")
	(GAME_BOY_COLOR, "Nintendo Game Boy Color")
	(NINTENDO_GAMECUBE, "Nintendo GameCube")
	(NINTENDO_WII, "Nintendo Wii")
	(NINTENDO_WII_U, "Nintendo Wii U")
	(PC, "PC")
	(SEGA_32X, "Sega 32X")
	(SEGA_CD, "Sega CD")
	(SEGA_DREAMCAST, "Sega Dreamcast")
	(SEGA_GAME_GEAR, "Sega Game Gear")
	(SEGA_GENESIS, "Sega Genesis")
	(SEGA_MASTER_SYSTEM, "Sega Master System")
	(SEGA_MEGA_DRIVE, "Sega Mega Drive")
	(SEGA_SATURN, "Sega Saturn")
	(PLAYSTATION, "Sony Playstation")
	(PLAYSTATION_2, "Sony Playstation 2")
	(PLAYSTATION_3, "Sony Playstation 3")
	(PLAYSTATION_4, "Sony Playstation 4")
	(PLAYSTATION_VITA, "Sony Playstation Vita")
	(PLAYSTATION_PORTABLE, "Sony PSP")
	(SUPER_NINTENDO, "Super Nintendo (SNES)")
	(TURBOGRAFX_16, "TurboGrafx 16")
	(WONDERSWAN, "WonderSwan")
	(WONDERSWAN_COLOR, "WonderSwan Color")
	(ZX_SPECTRUM, "Sinclair ZX Spectrum")
	(VIRTUAL_BOY, "Nintendo Virtual Boy")
	(GAME_AND_WATCH, "game-and-watch")
	(PC_ENGINE_CD, "TurboGrafx CD")
	(SUPERGRAFX, "TurboGrafx 16")
	(PRBOOM, "PC")
	(VECTREX, "Vectrex")
	(LUTRO, "PC")
	(CAVE_STORY, "PC")
	(ODYSSEY_2, "Magnavox Odyssey 2")
	(ZX_81, "Sinclair ZX Spectrum")
	(MOONLIGHT,"PC");

static const std::map<std::string, const char*> system_language_map = boost::assign::map_list_of
	("fr_FR", "")
	("de_DE", "forcelangue=de&")
	("es_ES", "forcelangue=es&")
	("en_US", "forcelangue=en&")
	("pt_BR", "forcelangue=pt&");

void screenscraper_generate_scraper_requests(const ScraperSearchParams& params, std::queue< std::unique_ptr<ScraperRequest> >& requests, 
	std::vector<ScraperSearchResult>& results)
{
	std::string path = "screenscraper.recalbox.com/api/thegamedb/GetGame.php?";
	std::string languageSystem = RecalboxConf::getInstance()->get("system.language");

	if((system_language_map.find(languageSystem)) != system_language_map.end())
	{
		path += (system_language_map.find(languageSystem)->second);
	}else{
		path += "forcelangue=en&";
	}

	std::string cleanName = params.game->getPath().filename().c_str();

	path += "name=" + HttpReq::urlEncode(cleanName);

	if(params.system->getPlatformIds().empty())
	{
		// no platform specified, we're done
		requests.push(std::unique_ptr<ScraperRequest>(new ScreenscraperRequest(results, path)));
	}else{
		// go through the list, we need to split this into multiple requests 
		// because TheGamesDB API either sucks or I don't know how to use it properly...
		std::string urlBase = path;
		auto& platforms = params.system->getPlatformIds();
		for(auto platformIt = platforms.begin(); platformIt != platforms.end(); platformIt++)
		{
			path = urlBase;
			auto mapIt = gamesdb_platformid_map.find(*platformIt);
			if(mapIt != gamesdb_platformid_map.end())
			{
				path += "&platform=";
				path += HttpReq::urlEncode(mapIt->second);
			}else{
				LOG(LogWarning) << "Screenscraper scraper warning - no support for platform " << getPlatformName(*platformIt);
			}

			requests.push(std::unique_ptr<ScraperRequest>(new ScreenscraperRequest(results, path)));
		}
	}
}

void ScreenscraperRequest::process(const std::unique_ptr<HttpReq>& req, std::vector<ScraperSearchResult>& results)
{
	assert(req->status() == HttpReq::REQ_SUCCESS);

	pugi::xml_document doc;
	pugi::xml_parse_result parseResult = doc.load(req->getContent().c_str());
	if(!parseResult)
	{
		std::stringstream ss;
		ss << "ScreenscraperRequest - Error parsing XML. \n\t" << parseResult.description() << "";
		std::string err = ss.str();
		setError(err);
		LOG(LogError) << err;
		return;
	}

	pugi::xml_node data = doc.child("Data");

	std::string baseImageUrl = data.child("baseImgUrl").text().get();

	pugi::xml_node game = data.child("Game");
	while(game && results.size() < MAX_SCRAPER_RESULTS)
	{
		ScraperSearchResult result;

		result.mdl.set("name", game.child("GameTitle").text().get());
		result.mdl.set("desc", game.child("Overview").text().get());

		boost::posix_time::ptime rd = string_to_ptime(game.child("ReleaseDate").text().get(), "%m/%d/%Y");
		result.mdl.setTime("releasedate", rd);

		result.mdl.set("developer", game.child("Developer").text().get());
		result.mdl.set("publisher", game.child("Publisher").text().get());
		result.mdl.set("genre", game.child("Genres").first_child().text().get());
		result.mdl.set("players", game.child("Players").text().get());

		if(Settings::getInstance()->getBool("ScrapeRatings") && game.child("Rating"))
		{
			result.mdl.set("rating", game.child("Rating").text().get());
		}

		pugi::xml_node images = game.child("Images");

		if(images)
		{
			pugi::xml_node art = images.find_child_by_attribute("boxart", "side", "front");

			if(art)
			{
				result.thumbnailUrl = baseImageUrl + art.attribute("thumb").as_string();
				result.imageUrl = baseImageUrl + art.text().get();
			}
		}

		results.push_back(result);
		game = game.next_sibling("Game");
	}
}
