package app;

import com.google.gson.Gson;
import org.apache.kafka.clients.consumer.Consumer;
import org.apache.kafka.clients.consumer.ConsumerRecords;
import org.apache.kafka.clients.consumer.KafkaConsumer;
import org.apache.kafka.common.errors.WakeupException;

import java.io.File;
import java.io.IOException;
import java.time.Duration;
import java.util.*;

/**
 * SimpleCardConsumer is a class responsible for consuming card data from a Kafka topic.
 * It utilizes the Kafka Consumer API to read serialized GSON representations of Card objects
 * from a specified Kafka topic and store them in a list.
 */
public class SimpleCardConsumer {

    private final boolean inDocker = new File("/.dockerenv").exists(); // DONT CHANGE

    private final List<Card> consumedCards = new ArrayList<>(); // DONT CHANGE

    private final Consumer<String, String> consumer; // DONT CHANGE
    private final Gson gson = new Gson(); // Add Gson instance for JSON deserialization


    /**
     * Default constructor that initializes the Kafka consumer using default properties.
     */
    public SimpleCardConsumer() {
        this.consumer = createKafkaConsumer();
    }

    /**
     * Constructor that allows passing a custom Kafka consumer instance.
     *
     * @param consumer A Kafka consumer instance to be used for consuming messages.
     */
    public SimpleCardConsumer(Consumer<String, String> consumer) {
        this.consumer = consumer;
    }

    /**
     * Creates and configures a Kafka consumer instance using properties loaded from a file.
     * This method checks if the application is running in a Docker environment and adjusts
     * the Kafka bootstrap servers accordingly.
     *
     * @return A configured KafkaConsumer instance.
     */
    public Consumer<String, String> createKafkaConsumer() { // DONT CHANGE
        try (var stream = Consumer.class.getClassLoader().getResourceAsStream("consumer.properties")) {
            Properties props = new Properties();
            props.load(stream);
            props.setProperty("client.id", "consumer-" + UUID.randomUUID());
            props.setProperty("group.instance.id", props.getProperty("client.id"));

            if (inDocker) {
                // Use Docker-specific Kafka bootstrap servers if in Docker
                props.setProperty("bootstrap.servers", props.getProperty("bootstrap.servers.docker"));
            }

            return new KafkaConsumer<>(props);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Consumes Card objects from the specified Kafka topic in an infinite loop.
     * Each consumed Card is deserialized and added to the consumedCards list.
     *
     * @param topic The Kafka topic from which to consume cards.
     */
    public void consumeCards(String topic) {

        try {
            // Subscribe to the topic
            consumer.subscribe(Collections.singletonList(topic));
            System.out.printf("Subscribed to topic: %s%n", topic);
            while (true) {
                // Poll for new records with a timeout
                ConsumerRecords<String, String> records = consumer.poll(Duration.ofMillis(100));

                // Process each record
                records.forEach(record -> {
                    try {
                        // Deserialize the JSON string to Card object
                        Card card = gson.fromJson(record.value(), Card.class);

                        // Add to the consumed cards list
                        synchronized (consumedCards) {
                            consumedCards.add(card);
                        }

                        System.out.printf("Consumed card -> Key: %s, Card: %s, Topic: %s, Partition: %d, Offset: %d%n",
                                record.key(), card, record.topic(), record.partition(), record.offset());
                    } catch (Exception e) {
                        System.err.printf("Error processing record: %s, Error: %s%n",
                                record.value(), e.getMessage());
                    }
                });

                // Commit offsets if needed (if enable.auto.commit=false in properties)
                if (!Boolean.parseBoolean(consumer.groupMetadata().groupId())) {
                    consumer.commitSync();
                }
            }
        } catch (WakeupException e) { // DONT CHANGE
            System.out.println("Shutting down...");
        } finally { // DONT CHANGE
            close();
        }
    }

    /**
     * close the consumer and release any resources held by the class
     */
    public void close() {
        // WRITE CODE HERE
        try {
            // Commit final offsets if needed
            consumer.commitSync();
        } finally {
            try {
                consumer.close();
                System.out.println("Consumer closed successfully");
            } catch (Exception e) {
                System.err.println("Error closing consumer: " + e.getMessage());
            }
        }
    }

    /**
     * Returns the list of consumed Card objects.
     *
     * @return A List containing the consumed Card objects.
     */
    public List<Card> getConsumedCards() {
        return consumedCards;
    }

    /**
     * This method can be used to stop the consumer gracefully.
     */
    public void stop() {
        consumer.wakeup(); // Wake up the consumer to exit the polling loop
    }

    public static void main(String[] args) {
        SimpleCardConsumer simpleCardConsumer = new SimpleCardConsumer();
        simpleCardConsumer.consumeCards("cards-topic");
    }
}
