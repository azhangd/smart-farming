import argparse
import configparser
import json
import logging
import os
import sys
import datetime

import paho.mqtt.client as mqtt
import psycopg2

logger = logging.getLogger(__name__)
logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

config = configparser.ConfigParser()
config.read('credentials.ini')

args = argparse.Namespace
ts_connection: str = ""

table_list = ["weather_aht20", "weather_scd30", "soil"]

def main():
    global args
    args = parse_args()

    global ts_connection
    ts_connection = "postgres://{}:{}@{}:{}/{}".format(args.ts_username, args.ts_password, args.ts_host, args.ts_port, args.ts_database)
    logger.debug("ts_connection: {}".format(ts_connection))

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    print(args.mqtt_host)
    print(args.mqtt_port)
    client.username_pw_set(args.mqtt_username, args.mqtt_password)
    client.connect(args.mqtt_host, args.mqtt_port, 60)

    global conn
    conn = psycopg2.connect(ts_connection, connect_timeout=3)

    # Infinite blocking loop. Reconnecting is handled by loop_forever()
    client.loop_forever()


# Callback on CONNACK response
def on_connect(client, userdata, flags, rc):
    logger.debug("Connected with result code {}".format(str(rc)))

    # Renews subscriptions if connection is lost
    client.subscribe(args.mqtt_topic)


# Callback on PUBLISH message received
def on_message(client, userdata, msg):
    logger.debug("Topic: {}, Message Payload: {}".format(msg.topic, str(msg.payload)))
    publish_message_to_db(msg)

def publish_message_to_db(message):
    message_payload = json.loads(message.payload)

    for key in message_payload:
        if key in table_list:
            query_columns = 'time, device_id, ' + ', '.join(message_payload[key])
            query_list = [
                str(datetime.datetime.utcfromtimestamp(message_payload["time"])), 
                message_payload["device_id"]
                ] + list(message_payload[key].values())
            print(query_list)
            query_values = str(query_list)[1:-1]
            sql = """INSERT INTO %s (%s) VALUES (%s);"""%(key, query_columns, query_values)
            print(sql)

            try:
                with conn.cursor() as curs:
                    try:
                        curs.execute(sql)
                        conn.commit()
                    except psycopg2.Error as error:
                        logger.error("Exception: {}".format(error.pgerror))
                    except Exception as error:
                        logger.error("Exception: {}".format(error))
            except psycopg2.OperationalError as error:
                logger.error("Exception: {}".format(error.pgerror))


# Read in command-line parameters
def parse_args():
    parser = argparse.ArgumentParser(description='Script arguments')
    parser.add_argument('--mqtt_topic', help='MQTT Topic', default=config['MQTT']['Topic'])
    parser.add_argument('--mqtt_host', help='MQTT Host', default=config['MQTT']['Host'])
    parser.add_argument('--mqtt_port', help='MQTT Port', type=int, default=int(config['MQTT']['Port']))
    parser.add_argument('--mqtt_username', help='MQTT Username', default=config['MQTT']['Username'])
    parser.add_argument('--mqtt_password', help='MQTT Password', default=config['MQTT']['Password'])
    parser.add_argument('--ts_host', help='TimescaleDB Host', default=config['TSDB']['Host'])
    parser.add_argument('--ts_port', help='TimescaleDB Port', type=int, default=int(config['TSDB']['Port']))
    parser.add_argument('--ts_username', help='TimescaleDB Username', default=config['TSDB']['Username'])
    parser.add_argument('--ts_password', help='TimescaleDB Password', default=config['TSDB']['Password'])
    parser.add_argument('--ts_database', help='Database', default=config['TSDB']['Database'])

    return parser.parse_args()

if __name__ == "__main__":
    main()