from __future__ import print_function
import time
import json
import sys
import random

import twitter
import paho.mqtt.client as mqtt
from unidecode import unidecode



#set this to true to have the script run persistently and sleep until it needs to grab another tweet
#set this to false if using this script via a cron type scheduled task
RUN_LOOP = False

#If using loop rather than cron type execution, this is the
#time in seconds for loop to sleep before grabbing another 
#tweet (default: 3600 seconds which is 1 hour)
LOOP_SLEEP_SECONDS = 3600



#Twitter developer api keys (set these according to your own api keys)

CONSUMER_KEY=		'***********************'
CONSUMER_SECRET=	'***********************'
ACCESS_TOKEN_KEY=	'***********************'
ACCESS_TOKEN_SECRET='***********************'




#MQTT Broker details (set these according to your own setup)

MQTT_HOST = "YOUR.MQTT.IP.HOST"
MQTT_USER = '' #optional
MQTT_PASS = '' #optional
MQTT_PORT = 1883
MQTT_TOPIC = '/your/mqtt/topic'

TAGS_FILE = "twitterTags.txt"








def readFile(filename):
	fp = open(filename,'r')
	tags = []
	for line in fp:
		tags += [line.strip()]
	fp.close()
	return tags



def main():
	api = twitter.Api(consumer_key=CONSUMER_KEY,
                  consumer_secret=CONSUMER_SECRET,
                  access_token_key=ACCESS_TOKEN_KEY,
                  access_token_secret=ACCESS_TOKEN_SECRET)


	#grab a hashtag from the tags file
	tags = readFile(TAGS_FILE)
	tag = tags[random.randrange(len(tags))]
	# print(tag)
	
	query = ''
	if tag[0] == '#':
		query = 'q=-filter%3Aretweets%23' + tag[1:]
	elif tag[0] == '@':
		query = 'q=from%3A' + tag[1:]


	#grab a tweet from the api
	timeline = api.GetSearch(raw_query=query + "%20&result_type=recent&count=1&tweet_mode=extended")
	dictVersion = timeline[0].AsDict()


	# print(json.dumps(timeline[0]._json, sort_keys=True, indent=4)) #view raw twitter data
	
	#parse the tweet data
	text = unidecode(dictVersion['full_text'])
	name = unidecode(dictVersion["user"]["name"])
	date = unidecode(dictVersion["created_at"])

	# print(dictVersion["created_at"])
	# print(dictVersion["user"]["name"])
	# print(text)

	

	mqttClient = mqtt.Client()
	mqttClient.username_pw_set(MQTT_USER, MQTT_PASS)
	mqttClient.connect(MQTT_HOST, MQTT_PORT, 60)
	mqttClient.publish(MQTT_TOPIC, "user: "+ name + '\n' + date + '\n\n' + text)
	





if __name__ == '__main__':
	if(RUN_LOOP == True):
		while(True):
			main()
			time.sleep(LOOP_SLEEP_SECONDS)

	else:
		main()


