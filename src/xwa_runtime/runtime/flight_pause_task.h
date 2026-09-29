#ifndef XWA_RUNTIME_FLIGHT_PAUSE_TASK_H
#define XWA_RUNTIME_FLIGHT_PAUSE_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Alt+P hands off at the end of the current flight frame. The task preserves
 * the original pause effects while allowing the host to deliver fresh input. */
void XwaFlightPauseTask_Request(unsigned int playerIdx);
/* Returns nonzero for every paused frame, including entry and resume.
 * The caller must not advance simulation on those frames. */
int XwaFlightPauseTask_Tick(void);
int XwaFlightPauseTask_IsActive(void);
void XwaFlightPauseTask_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
