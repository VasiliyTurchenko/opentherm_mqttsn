#ifndef TASK_TOKENS_H
#define TASK_TOKENS_H

#define MAGIC_SEED	1

#define	LAN_POLL_TASK_MAGIC	(uint32_t)(MAGIC_SEED)
#define LOGGER_TASK_MAGIC	(uint32_t)(MAGIC_SEED + 1)
#define MANCHESTER_TASK_MAGIC	(uint32_t)(MAGIC_SEED + 2)
#define PUB_TASK_MAGIC		(uint32_t)(MAGIC_SEED + 3)
#define SUB_TASK_MAGIC		(uint32_t)(MAGIC_SEED + 4)
#define SERVICE_TASK_MAGIC	(uint32_t)(MAGIC_SEED + 5)

#endif // TASK_TOKENS_H
