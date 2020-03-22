/*
AshamaneCoreLegacy-Legion
cb255034ad1c6438ab2d5618ec09c329765b9d4e
2020-03-17
*/


/*!40101 SET NAMES utf8 */;

DELETE FROM `race_unlock_requirement` WHERE `raceID`=27;
DELETE FROM `race_unlock_requirement` WHERE `raceID`=28;
DELETE FROM `race_unlock_requirement` WHERE `raceID`=29;
DELETE FROM `race_unlock_requirement` WHERE `raceID`=30;

insert into `race_unlock_requirement` (`raceID`, `expansion`, `achievementId`) values('27','6','0');
insert into `race_unlock_requirement` (`raceID`, `expansion`, `achievementId`) values('28','6','0');
insert into `race_unlock_requirement` (`raceID`, `expansion`, `achievementId`) values('29','6','0');
insert into `race_unlock_requirement` (`raceID`, `expansion`, `achievementId`) values('30','6','0');

