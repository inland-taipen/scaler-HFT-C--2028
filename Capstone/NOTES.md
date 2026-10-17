## D1: Integer ticks instead of double prices
Doubles approximate: 0.29 * 100 = 28.999..., which truncates to 28 and
misplaces the order by one tick. The error is compiler- and platform-
dependent, so it can't be tested away. Prices are stored as int32 ticks
(price * 100, rounded via std::llround) and used as integers everywhere.
