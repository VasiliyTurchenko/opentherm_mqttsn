/** @file mqtt_config_helper.h
 *  @brief A helper function for configuration MQTT-SN pub and sub tasks
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 04-Apr-2019
 */


#ifndef MQTT_CONFIG_HELPER_H
#define MQTT_CONFIG_HELPER_H




void mqtt_initialize(uint32_t task_magic,
                        const struct MQTT_parameters * params_pool,
                        struct MQTT_topic_para * working_set,
                        cfg_pool_t * ip_pool);

#endif // MQTT_CONFIG_HELPER_H
