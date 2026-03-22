#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H

/* Node ID to peripheral population mapping.
 *
 * Node IDs 1–5 match the DIP switch value read at boot (PB0/PB1/PB2).
 * The same firmware binary runs on all five boards; these macros gate
 * hardware initialisation so unused peripherals are never touched.
 *
 * Population matrix (● = populated):
 *
 *   ID | Board         | TMC | Servo | ToF | Pot | LED
 *   ---+---------------+-----+-------+-----+-----+----
 *   1  | X axis        |  ●  |       |  ●  |     |
 *   2  | Z axis        |  ●  |       |  ●  |     |
 *   3  | A axis        |  ●  |       |  ●  |  ●  |  ●
 *   4  | Pincher       |     |   ●   |  ●  |     |
 *   5  | Player        |     |   ●   |     |     |  ●
 */

#define NODE_HAS_TMC(id)    ((id) >= 1 && (id) <= 3)
#define NODE_HAS_SERVO(id)  ((id) >= 4)
#define NODE_HAS_TOF(id)    ((id) >= 1 && (id) <= 4)
#define NODE_HAS_POT(id)    ((id) == 3)
#define NODE_HAS_LED(id)    ((id) == 3 || (id) == 5)

#endif /* NODE_CONFIG_H */
