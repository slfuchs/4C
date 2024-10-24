// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "4C_shell_kl_nurbs.hpp"

#include "4C_unittest_utils_assertions_test.hpp"


namespace
{
  using namespace FourC;
  TEST(KirchhoffLoveShellTest, TestResAndStiff)
  {
    // Shell material parameter
    const double young = 10.0;
    const double nu = 0.3;
    const double thickness = 0.05;

    // Integration rule
    const auto intpoints_xi = Core::FE::IntegrationPoints1D(
        Core::FE::num_gauss_points_to_gauss_rule<Core::FE::CellType::line2>(4));
    const auto intpoints_eta = Core::FE::IntegrationPoints1D(
        Core::FE::num_gauss_points_to_gauss_rule<Core::FE::CellType::line2>(3));

    // NURBS stuff
    std::vector<Core::LinAlg::SerialDenseVector> knots(2);
    knots[0].size(6);
    knots[0](0) = 0.0;
    knots[0](1) = 0.2;
    knots[0](2) = 0.4;
    knots[0](3) = 0.6;
    knots[0](4) = 0.8;
    knots[0](5) = 1.0;
    knots[1].size(6);
    knots[1](0) = 0.0;
    knots[1](1) = 0.0;
    knots[1](2) = 0.5;
    knots[1](3) = 1.0;
    knots[1](4) = 1.0;
    knots[1](5) = 1.0;

    std::array<double, 9> weights_array = {1, 1, 1, 1, 1, 1, 1, 1.25, 1};
    const Core::LinAlg::Matrix<9, 1> weights(weights_array.data(), true);

    // Reference position of the control points
    std::array<double, 27> X_array = {-0.599999999999999978, 0, 0.599999999999999978,
        -0.599999999999999978, 0, 0.599999999999999978, -0.599999999999999978,
        0.1000000000000000056, 0.599999999999999978, -0.25, -0.25, -0.25, 0.25, 0.25, 0.25, 0.5,
        0.599999999999999978, 0.5, 0, 0, 0, 0, 0, 0, 0, 0.5, 0};
    const Core::LinAlg::Matrix<9, 3> X(X_array.data(), true);

    // State vector of the element
    std::array<double, 27> q_array = {-0.0073460039068901465675, -0.0090024661063670703004,
        0.098265647494483521207, -0.016292436705818230669, -0.023538879470180698594,
        0.20261356200430805097, -0.030797522545841751057, -0.037643834523907147849,
        0.33468350693831616649, 0.0094783058405895157655, -0.010348836244285019442,
        0.066347117623620485705, 0.0021877761594420431189, -0.0229380975449950536,
        0.17181312918765817566, -0.0091849699740537294607, -0.040533291823822398259,
        0.28577345407683141243, 0.016706658745077065248, -0.012233619465664861969,
        0.057688267219970876254, -0.077207122339025990865, 0.002335258998694294142,
        0.16514038627451746444, -0.0016138033692716085427, -0.039545394662099921101,
        0.27079254279844588993};
    const Core::LinAlg::Matrix<27, 1> q(q_array.data(), true);

    // Reference solutions
    std::array<double, 27> res_ref_array = {-4.002528200137324e-05, -1.966105411997185e-05,
        -1.415652978827616e-05, 1.41590910579256e-05, 2.262400944731972e-05, 1.527448724724087e-05,
        -9.679495420635895e-06, 4.125921472621884e-05, 2.040790281812236e-05, -9.59658528313565e-05,
        -5.196119173075736e-05, -7.213414614232678e-05, 0.0002034700236036548,
        -9.998306069097489e-05, -8.136234010660518e-05, -2.643036670528856e-05,
        4.210067724333785e-05, 2.727544093196633e-05, -0.0001021884891980149, 7.793819852950685e-05,
        2.855207732261376e-05, 9.828349407222035e-05, 3.549729047886519e-05, 7.37790361594396e-05,
        -4.16231225771317e-05, -4.78140838835444e-05, 2.364071557825173e-06};
    std::array<std::array<double, 27>, 27> stiff_ref_array = {//
        {{0.01066474017074286, 0.004425680537147158, 0.002886325982184954, 0.002486382648403143,
             0.004192068489251147, 0.001206090547616003, -0.003583949235756761,
             6.317930515217448e-05, -0.0006810219269995798, 0.01470787632043459,
             0.003544887812902195, 0.004664258764339195, -0.00785306862735566,
             -0.006782745857186526, -0.002705925638227671, -0.008482577180424958,
             -0.001820445838044202, -0.002054621270462283, -9.102379207545207e-05,
             0.0008443311558448538, 0.0005257412891976772, -0.006039183708042182,
             -0.003696053144021869, -0.003120681948509339, -0.001809196595925577,
             -0.0007709024610449317, -0.0007201657991389561},
            {0.004425680537147158, 0.01052717591309812, 0.00280814906985721, 0.00555027174216322,
                0.01661104157867287, 0.004584525645834499, 0.0003343932743916027,
                2.621687416399081e-05, 0.0001324533273552172, 0.002500982208474601,
                0.001808626604967611, 0.002431089377985171, -0.006560897818250847,
                -0.005220835113076198, 0.0009189443317285995, -0.001568614381260922,
                -0.002655907115728526, -0.0007382792918433903, 0.000391846688571699,
                -0.005939769123587756, -0.002317033963585992, -0.004296484659636398,
                -0.01412198434519288, -0.007155320834193678, -0.0007771775916001156,
                -0.001034565273317245, -0.0006645276631376376},
            {0.002886325982184954, 0.00280814906985721, 0.001585504210756849, 0.001469341327919744,
                0.004298935990492315, 0.001796054105457516, -0.0006245493155671249,
                7.957350510971665e-05, -8.676227396275056e-05, 0.004419690718248661,
                0.002672426598320454, 0.002648372871943527, -0.002459654253028504,
                0.0008126798263837656, 0.0009308794242519495, -0.001944391334606364,
                -0.0007916942818112739, -0.0005990095594764446, 0.0003061115687698953,
                -0.002172330455026758, -0.001054531917591287, -0.003345377757597302,
                -0.007038865700811654, -0.004715747970782225, -0.0007074969363239599,
                -0.0006688745525137766, -0.0005047588905971373},
            {0.002486382648403141, 0.00555027174216322, 0.001469341327919744, 0.03827163506527873,
                -0.002323888067684017, 0.006379123151722229, 0.003103453907080418,
                -0.005927332215943553, -0.0006678134463501418, -0.01229040453302025,
                0.01760123075843605, 0.002230915165052564, 0.0177917576516287,
                -0.003704843007117227, 0.001721887301244821, -0.008763289307868046,
                -0.01732279050059898, -0.006469548604063434, -0.00994943813180647,
                0.007743444726118332, 0.001106332974159863, -0.02317675996208586,
                0.003815768486007785, -0.002356248013365666, -0.007473337337610363,
                -0.005431861921381609, -0.003413989856319979},
            {0.004192068489251147, 0.01661104157867287, 0.004298935990492314, -0.002323888067684017,
                0.08217022055596523, 0.0165569527348933, -0.004565184915561691, 0.01828329318241961,
                0.002925535916029136, 0.01529864620405519, -0.01192292202014994,
                0.003336467346054276, -0.003716756203596543, -0.009941912261323156,
                0.01119940143935632, -0.01500269181789828, -0.004819963040375168,
                0.0001019925941900064, 0.007323198159497998, -0.01651379196921403,
                -0.005645803585117939, 0.003798869047588424, -0.06172803004029882,
                -0.02695547423975066, -0.00500426089565223, -0.01213793598569662,
                -0.005818008196146755},
            {0.001206090547616003, 0.0045845256458345, 0.001796054105457516, 0.006379123151722228,
                0.0165569527348933, 0.007878477745073621, -0.0004085881659924594,
                0.002682861868626785, 0.001198638224003741, 0.001442892315441699,
                0.003867174983283928, 0.002296451513267564, 0.001724778040215956,
                0.0110946654916025, 0.009219936011551364, -0.00567488070691697,
                -0.0002750291348730066, 0.0004522143462549875, 0.0008034397824407253,
                -0.005527893702894041, -0.003056449874041509, -0.002367719782761652,
                -0.02711316349980355, -0.01624677491849626, -0.003105135181765531,
                -0.005870094386670414, -0.003538547153071021},
            {-0.003583949235756761, 0.0003343932743916027, -0.0006245493155671249,
                0.003103453907080418, -0.00456518491556169, -0.0004085881659924599,
                0.0113219051806201, -0.005572851465958321, 0.0009815378205333635,
                -0.008958198182766993, 0.002973834052103556, -0.0008964661933354896,
                -0.008594595867834761, 0.006622808812641808, -0.0006759586428094375,
                0.01673152830918982, -0.006152259188733362, 0.0002329222806441594,
                -0.00228433711336083, 0.001244363983917119, 0.0001928199082041841,
                -0.008246595803784068, 0.005761148910802646, 0.001628711170599161,
                0.0005107888066130759, -0.0006462534636033628, -0.0004304288622763559},
            {6.31793051521745e-05, 2.621687416399066e-05, 7.957350510971665e-05,
                -0.005927332215943551, 0.01828329318241961, 0.002682861868626785,
                -0.005572851465958321, 0.01217281023940297, 0.001189309270293969,
                0.002721627814025805, -0.003628106204805269, -0.000231553087939497,
                0.00638639819389949, -0.007700846128963513, 0.003083007861879927,
                -0.005101232032919799, 0.003936583223614927, 0.002081142448043517,
                0.001249774746239408, -0.001641192050853689, -0.0006174157537369956,
                0.006362197752636672, -0.01633294475971302, -0.006329977242078645,
                -0.0001817620971318777, -0.005115814375266007, -0.001936948870198778},
            {-0.0006810219269995797, 0.0001324533273552173, -8.676227396275058e-05,
                -0.0006678134463501419, 0.002925535916029137, 0.001198638224003741,
                0.0009815378205333635, 0.001189309270293969, 0.0009724453127409295,
                -0.001007327093630188, -0.0001849970819332795, -0.00032123058103207,
                -0.0009309097233037567, 0.00306759309340502, 0.001563619370153206,
                0.0004778005589500995, 0.001914231098862241, 0.001609123925069348,
                0.0001789784461210878, -0.0006235279757953281, -0.0003427110698225933,
                0.001850692166416466, -0.00644967641852422, -0.003528605685234305,
                -0.0002019368017373497, -0.001970921229692758, -0.001064517221915505},
            {0.01470787632043459, 0.002500982208474601, 0.004419690718248661, -0.01229040453302025,
                0.01529864620405519, 0.001442892315441699, -0.008958198182766993,
                0.002721627814025805, -0.001007327093630188, 0.05322282345658665,
                -0.006344399646151273, 0.01323748042609856, -0.01018713496954585,
                -0.0001603192530346688, -0.001834889079298821, -0.02530333979157076,
                0.002048647777117867, -0.003951467706776682, 0.0110323237598584,
                0.002771751050206043, 0.005145868207991092, -0.01085087297819441,
                -0.0157792632961308, -0.01355085248619486, -0.01137307308178138,
                -0.00305767285856277, -0.003901395301879463},
            {0.003544887812902194, 0.001808626604967613, 0.002672426598320454, 0.01760123075843604,
                -0.01192292202014993, 0.003867174983283928, 0.002973834052103556,
                -0.003628106204805269, -0.0001849970819332795, -0.006344399646151275,
                0.02895662361558652, 0.01151651853704633, 0.0007740996630542171,
                0.02240193896156763, 0.01415801446938916, 0.002235957849948537,
                -0.005754863633334206, -0.001624416900394328, 0.000809754578381961,
                -0.01001114592714003, -0.006557800201771601, -0.01849367506180224,
                -0.01897562497468918, -0.02107787873388428, -0.003101690006872991,
                -0.00287452642200315, -0.002769041670056388},
            {0.004664258764339195, 0.002431089377985172, 0.002648372871943528, 0.002230915165052565,
                0.003336467346054276, 0.002296451513267564, -0.0008964661933354893,
                -0.0002315530879394969, -0.00032123058103207, 0.01323748042609856,
                0.01151651853704633, 0.01264671389564246, -0.0005839140410781124,
                0.01371916487642316, 0.01110102801531694, -0.003682999561229282,
                -0.001662350331942372, -0.001610728198136848, 0.00385906755072951,
                -0.005753119133114474, -0.003286686795281193, -0.01498294896553438,
                -0.02055386090753516, -0.02051616693973229, -0.003845393145042563,
                -0.002802356676977443, -0.002957753781988095},
            {-0.007853068627355657, -0.006560897818250846, -0.002459654253028504,
                0.0177917576516287, -0.003716756203596545, 0.001724778040215958,
                -0.008594595867834764, 0.00638639819389949, -0.0009309097233037567,
                -0.01018713496954585, 0.0007740996630542222, -0.0005839140410781139,
                0.1286875586985061, -0.01737188114642698, 0.01395000167533094,
                -0.009316000528315159, -0.002238563227114351, -0.005006132817988776,
                -0.0440955731699121, 0.03828100929261159, 0.007552962403273552,
                -0.03467430206944649, 0.01160082533652994, 0.001423925422002738,
                -0.03175864111772481, -0.02715423409070652, -0.01567105670542403},
            {-0.006782745857186524, -0.005220835113076196, 0.000812679826383765,
                -0.003704843007117226, -0.009941912261323158, 0.0110946654916025,
                0.00662280881264181, -0.007700846128963511, 0.003067593093405019,
                -0.0001603192530346684, 0.02240193896156763, 0.01371916487642316,
                -0.01737188114642698, 0.1476919330400874, 0.07404018195690669,
                -0.001252384634488054, 0.02238338737544448, 0.01446632601023566, 0.0365354135696418,
                -0.04686168732559051, -0.02023137949251442, 0.01148347168416409,
                -0.0985596490671139, -0.07794926025178864, -0.02536952016819424,
                -0.02419232948103227, -0.01901997151065372},
            {-0.002705925638227671, 0.0009189443317285995, 0.0009308794242519495,
                0.001721887301244821, 0.01119940143935631, 0.009219936011551366,
                -0.0006759586428094375, 0.003083007861879927, 0.001563619370153206,
                -0.001834889079298819, 0.01415801446938916, 0.01110102801531694,
                0.01395000167533094, 0.07404018195690668, 0.06192000991090403,
                -0.003701410221963678, 0.01453304823990744, 0.01102568476240993,
                0.005858781649191287, -0.01963954229589564, -0.01572782052571899,
                0.001311049896991224, -0.07917498486430824, -0.06456884706173234,
                -0.01392353694045866, -0.01911807113896422, -0.01546448990713609},
            {-0.00848257718042496, -0.001568614381260922, -0.001944391334606363,
                -0.008763289307868048, -0.01500269181789828, -0.00567488070691697,
                0.01673152830918983, -0.0051012320329198, 0.0004778005589500997,
                -0.02530333979157076, 0.002235957849948537, -0.003682999561229282,
                -0.009316000528315157, -0.001252384634488052, -0.003701410221963676,
                0.05880271110773537, -0.004256121537800403, 0.002027218355425231,
                -0.01407573879267886, 0.006037842305834413, 0.001164484829537317,
                -0.02424376401322381, 0.02274738454348096, 0.01302113022455925, 0.01465047019715639,
                -0.003840140294896457, -0.001686952143755609},
            {-0.001820445838044202, -0.002655907115728526, -0.0007916942818112738,
                -0.01732279050059898, -0.004819963040375169, -0.0002750291348730067,
                -0.006152259188733362, 0.003936583223614928, 0.001914231098862241,
                0.002048647777117868, -0.005754863633334206, -0.001662350331942373,
                -0.00223856322711435, 0.02238338737544449, 0.01453304823990744,
                -0.004256121537800401, 0.02903950407515157, 0.01393122781356453, 0.006075835406911,
                -0.005887531397289938, -0.003075614060811247, 0.02546776836441982,
                -0.02994085745051413, -0.02029370800190576, -0.001802071256157396,
                -0.006300352036969019, -0.004280111340990551},
            {-0.002054621270462283, -0.0007382792918433903, -0.0005990095594764447,
                -0.006469548604063433, 0.0001019925941900074, 0.0004522143462549878,
                0.0002329222806441588, 0.002081142448043517, 0.001609123925069348,
                -0.003951467706776682, -0.001624416900394327, -0.001610728198136848,
                -0.005006132817988776, 0.01446632601023566, 0.01102568476240993,
                0.002027218355425232, 0.01393122781356453, 0.01014282809521859, 0.00109880117820766,
                -0.003124814523001531, -0.002470546211620882, 0.01443938408342724,
                -0.02085336957163784, -0.01562960974058364, -0.0003165554984131151,
                -0.004239808579156634, -0.002919957419135044},
            {-9.102379207545217e-05, 0.0003918466885716996, 0.0003061115687698953,
                -0.00994943813180647, 0.007323198159497998, 0.0008034397824407256,
                -0.00228433711336083, 0.001249774746239408, 0.0001789784461210878,
                0.0110323237598584, 0.0008097545783819582, 0.003859067550729512,
                -0.0440955731699121, 0.0365354135696418, 0.00585878164919129, -0.01407573879267886,
                0.006075835406911001, 0.00109880117820766, 0.06649758046250248,
                -0.03369304932110889, 0.004658354882369018, 0.01485766046004998,
                -0.02136743920567529, -0.01488574490069435, -0.02189145368257715,
                0.002674665377540316, -0.00187779015713484},
            {0.0008443311558448543, -0.005939769123587756, -0.002172330455026756,
                0.007743444726118332, -0.01651379196921402, -0.005527893702894039,
                0.001244363983917119, -0.001641192050853689, -0.000623527975795328,
                0.002771751050206044, -0.01001114592714003, -0.005753119133114473,
                0.03828100929261159, -0.04686168732559051, -0.01963954229589564,
                0.006037842305834413, -0.005887531397289938, -0.003124814523001531,
                -0.03369304932110889, 0.04601266749336069, 0.01596471540446069,
                -0.02528288899330925, 0.04476666455143281, 0.02326690514849181,
                0.002053195799885772, -0.00392421425111755, -0.002390392467224732},
            {0.0005257412891976773, -0.002317033963585993, -0.001054531917591287,
                0.001106332974159863, -0.00564580358511794, -0.003056449874041508,
                0.0001928199082041841, -0.0006174157537369956, -0.0003427110698225933,
                0.005145868207991092, -0.0065578002017716, -0.003286686795281192,
                0.007552962403273556, -0.02023137949251442, -0.01572782052571899,
                0.001164484829537317, -0.003075614060811247, -0.002470546211620882,
                0.004658354882369019, 0.0159647154044607, 0.01647217770705295, -0.01795437644791877,
                0.02474791224680432, 0.01235201523076727, -0.002392188046813938,
                -0.002267580593726818, -0.002885446543743755},
            {-0.00603918370804218, -0.004296484659636397, -0.003345377757597302,
                -0.02317675996208586, 0.003798869047588424, -0.002367719782761652,
                -0.008246595803784066, 0.006362197752636672, 0.001850692166416466,
                -0.01085087297819441, -0.01849367506180224, -0.01498294896553438,
                -0.0346743020694465, 0.01148347168416409, 0.001311049896991224, -0.0242437640132238,
                0.02546776836441982, 0.01443938408342724, 0.01485766046004997, -0.02528288899330925,
                -0.01795437644791877, 0.09717756240340311, -0.01871681560178624,
                0.00192754943976713, -0.004803744328676276, 0.01967755746772512,
                0.01912174736721004},
            {-0.003696053144021869, -0.01412198434519288, -0.007038865700811654,
                0.003815768486007783, -0.06172803004029882, -0.02711316349980355,
                0.005761148910802647, -0.01633294475971302, -0.006449676418524219,
                -0.0157792632961308, -0.01897562497468918, -0.02055386090753517,
                0.01160082533652994, -0.09855964906711388, -0.07917498486430828,
                0.02274738454348096, -0.02994085745051413, -0.02085336957163784,
                -0.0213674392056753, 0.04476666455143281, 0.02474791224680431, -0.01871681560178623,
                0.1678619371175771, 0.1173725205549073, 0.01563444397079286, 0.02703048896851202,
                0.01906348816090904},
            {-0.00312068194850934, -0.007155320834193678, -0.004715747970782226,
                -0.002356248013365666, -0.02695547423975066, -0.01624677491849626,
                0.001628711170599162, -0.006329977242078645, -0.003528605685234304,
                -0.01355085248619486, -0.02107787873388428, -0.02051616693973229,
                0.001423925422002739, -0.07794926025178865, -0.06456884706173233,
                0.01302113022455926, -0.02029370800190577, -0.01562960974058363,
                -0.01488574490069435, 0.02326690514849182, 0.01235201523076726,
                0.001927549439767134, 0.1173725205549073, 0.09820661109439767, 0.01591221109183592,
                0.01912219360020255, 0.01464712599139613},
            {-0.001809196595925577, -0.0007771775916001156, -0.0007074969363239598,
                -0.007473337337610364, -0.00500426089565223, -0.003105135181765532,
                0.000510788806613076, -0.0001817620971318774, -0.0002019368017373496,
                -0.01137307308178138, -0.003101690006872992, -0.003845393145042564,
                -0.03175864111772481, -0.02536952016819424, -0.01392353694045866,
                0.01465047019715639, -0.001802071256157397, -0.0003165554984131125,
                -0.02189145368257715, 0.002053195799885772, -0.002392188046813938,
                -0.004803744328676276, 0.01563444397079286, 0.01591221109183592,
                0.06394818714052611, 0.01854884224493022, 0.008580031458719202},
            {-0.0007709024610449316, -0.001034565273317246, -0.0006688745525137768,
                -0.00543186192138161, -0.01213793598569663, -0.005870094386670415,
                -0.0006462534636033629, -0.005115814375266009, -0.001970921229692758,
                -0.00305767285856277, -0.002874526422003151, -0.002802356676977443,
                -0.02715423409070652, -0.02419232948103228, -0.01911807113896423,
                -0.003840140294896458, -0.006300352036969019, -0.004239808579156633,
                0.002674665377540317, -0.003924214251117548, -0.002267580593726817,
                0.01967755746772512, 0.02703048896851203, 0.01912219360020255, 0.01854884224493022,
                0.02854924885688985, 0.01781551355749952},
            {-0.000720165799138956, -0.0006645276631376376, -0.0005047588905971373,
                -0.003413989856319979, -0.005818008196146755, -0.003538547153071021,
                -0.0004304288622763558, -0.001936948870198779, -0.001064517221915506,
                -0.003901395301879463, -0.002769041670056388, -0.002957753781988095,
                -0.01567105670542403, -0.01901997151065372, -0.01546448990713609,
                -0.001686952143755609, -0.004280111340990552, -0.002919957419135044,
                -0.00187779015713484, -0.002390392467224733, -0.002885446543743755,
                0.01912174736721004, 0.01906348816090904, 0.01464712599139613, 0.0085800314587192,
                0.01781551355749952, 0.01468834492619052}}};
    Core::LinAlg::SerialDenseVector res_ref;
    res_ref.size(27);
    Core::LinAlg::SerialDenseMatrix stiff_ref;
    stiff_ref.shape(27, 27);
    for (unsigned int i_row = 0; i_row < 27; i_row++)
    {
      res_ref(i_row) = res_ref_array[i_row];
      for (unsigned int i_col = 0; i_col < 27; i_col++)
      {
        stiff_ref(i_row, i_col) = stiff_ref_array[i_row][i_col];
      }
    }

    // Initialize residuum and Jacobian
    Core::LinAlg::SerialDenseVector res;
    res.size(27);
    Core::LinAlg::SerialDenseMatrix stiff;
    stiff.shape(27, 27);

    // Evaluate residuum and Jacobian at the same time
    res.putScalar(0.0);
    stiff.putScalar(0.0);
    Discret::Elements::KirchhoffLoveShellNurbs::evaluate_residuum_and_jacobian_auto_generated(
        young, nu, thickness, intpoints_xi, intpoints_eta, knots, weights, X, q, res, stiff);
    FOUR_C_EXPECT_NEAR(res, res_ref, 1e-12);
    FOUR_C_EXPECT_NEAR(stiff, stiff_ref, 1e-12);

    // Evaluate residuum
    res.putScalar(0.0);
    FourC::Discret::Elements::KirchhoffLoveShellNurbs::evaluate_residuum_auto_generated(
        young, nu, thickness, intpoints_xi, intpoints_eta, knots, weights, X, q, res);
    FOUR_C_EXPECT_NEAR(res, res_ref, 1e-12);
  }
}  // namespace
