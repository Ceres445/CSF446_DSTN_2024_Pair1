package app;

import com.google.gson.Gson;
import org.apache.kafka.clients.producer.KafkaProducer;
import org.apache.kafka.clients.producer.Producer;
import org.apache.kafka.clients.producer.ProducerRecord;
import org.apache.kafka.clients.producer.RecordMetadata;

import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.Properties;
import java.util.UUID;
import java.util.concurrent.Future;

/**
 * SimpleCardProducer is a class responsible for producing card data to a Kafka topic.
 * It utilizes the Kafka Producer API to send serialized GSON representations of Card objects
 * to a specified Kafka topic. This class is suitable for both standalone and Docker environments.
 */
public class SimpleCardProducer {

    private final boolean inDocker = new File("/.dockerenv").exists(); // DONT CHANGE

    private final Producer<String, String> producer; // DONT CHANGE
    private final Gson gson = new Gson(); // Add Gson instance for JSON serialization
    /**
     * Default constructor that initializes the Kafka producer using properties.
     */
    public SimpleCardProducer() { // DONT CHANGE
        this.producer = createKafkaProducer();
    }

    /**
     * Constructor that allows passing a custom Kafka producer instance.
     *
     * @param producer A Kafka producer instance to be used for producing messages.
     */
    public SimpleCardProducer(Producer<String, String> producer) { // DONT CHANGE
        this.producer = producer;
    }

    /**
     * Creates and configures a Kafka producer instance using properties loaded from a file.
     * This method checks if the application is running in a Docker environment and adjusts
     * the Kafka bootstrap servers accordingly.
     *
     * @return A configured KafkaProducer instance.
     */
    public Producer<String, String> createKafkaProducer() { // DONT CHANGE
        try (var stream = Producer.class.getClassLoader().getResourceAsStream("producer.properties")) {
            Properties props = new Properties();
            props.load(stream);
            props.setProperty("client.id", "producer-" + UUID.randomUUID());
            if (inDocker) {
                // Use Docker-specific Kafka bootstrap servers if in Docker
                props.setProperty("bootstrap.servers", props.getProperty("bootstrap.servers.docker"));
            }
            System.out.println("Producer initialised:");
            return new KafkaProducer<>(props);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Produces a single Card object to the specified Kafka topic
     *
     * @param card    The Card object to be produced.
     * @param topic   The Kafka topic to which the card will be sent.
     */
    public void produceCard(Card card, String topic) {
        // WRITE CODE HERE
        try {
            String cardJson = gson.toJson(card);
            ProducerRecord<String, String> record = new ProducerRecord<>(
                    topic,
                    String.valueOf(card.id),  // Use card id as key
                    cardJson
            );

            Future<RecordMetadata> future = producer.send(record, (metadata, exception) -> {
                if (exception != null) {
                    System.err.println("Error producing card: " + exception.getMessage());
                } else {
                    System.out.printf("Produced card %d to topic %s partition %d offset %d%n",
                            card.id, metadata.topic(), metadata.partition(), metadata.offset());
                }
            });
        } catch (Exception e) {
            System.err.println("Error serializing/producing card: " + e.getMessage());
        }
    }

    /**
     * Produces a single Card object to the specified Kafka topic and partition.
     *
     * @param card    The Card object to be produced.
     * @param topic   The Kafka topic to which the card will be sent.
     * @param partition The partition of the topic to which the card will be sent.
     */
    public void produceCard(Card card, String topic, int partition) {
        // WRITE CODE HERE
        try {
            String cardJson = gson.toJson(card);
            ProducerRecord<String, String> record = new ProducerRecord<>(
                    topic,
                    partition,
                    String.valueOf(card.id),  // Use card id as key
                    cardJson
            );

            Future<RecordMetadata> future = producer.send(record, (metadata, exception) -> {
                if (exception != null) {
                    System.err.println("Error producing card to partition " + partition + ": " + exception.getMessage());
                } else {
                    System.out.printf("Produced card %d to topic %s partition %d offset %d%n",
                            card.id, metadata.topic(), metadata.partition(), metadata.offset());
                }
            });
        } catch (Exception e) {
            System.err.println("Error serializing/producing card to partition " + partition + ": " + e.getMessage());
        }
    }


    /**
     * Produces multiple Card objects to the specified Kafka topic
     *
     * @param cards The list of Card objects to be produced.
     * @param topic The Kafka topic to which the cards will be sent.
     */
    public void produceCards(List<Card> cards, String topic) {
        // WRITE CODE HERE
        for(Card card: cards) {
            produceCard(card, topic);
        }
    }

    /**
     * Flushes pending records to Kafka broker.
     */
    public void flush() {
        // WRITE CODE HERE
        producer.flush();
    }

    /**
     * close the producer and release any resources held by the class
     */
    public void close() {
        // WRITE CODE HERE
        try {
            flush();
            producer.close();
        } catch (Exception e) {
            System.err.println("Error closing producer: " + e.getMessage());
        }
    }


    public static void main(String[] args) {
        SimpleCardProducer simpleCardProducer = new SimpleCardProducer();

        int numCards = 100;
        List<Card> cards = Card.getRandomCards(numCards);

        long start = System.currentTimeMillis();
        simpleCardProducer.produceCards(cards, "cards-topic");
        simpleCardProducer.flush();
        long end = System.currentTimeMillis();

        System.out.println("Produced " + numCards + " cards in " + (end - start) + "ms");

        simpleCardProducer.close();
    }
}

