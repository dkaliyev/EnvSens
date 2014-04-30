using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Web.Http;
using WebApiApp.Hubs;
using WebApiApp.Models;
using MongoDB.Bson;
using MongoDB.Driver;
using MongoDB.Driver.Builders;
using MongoDB.Driver.Linq;

namespace WebApiApp.Controllers
{
    public class ReadingsController : ControllerWithHub<SensorsHub>
    {
        public IEnumerable<SensorReading> Get()
        {
            var connectionString = "mongodb://localhost";
            var client = new MongoClient(connectionString);
            var server = client.GetServer();
            var db = server.GetDatabase("sensorData");
            var collection = db.GetCollection<SensorReading>("SensorReadings20");
            var list = collection.FindAllAs<SensorReading>().SetSortOrder(SortBy.Descending("date")).SetLimit(20);
            return list;
        }

        public SensorReading Get(int Id)
        {
            var connectionString = "mongodb://localhost";
            var client = new MongoClient(connectionString);
            var server = client.GetServer();
            var db = server.GetDatabase("sensorData");
            var collection = db.GetCollection<SensorReading>("SensorReadings20");
            SensorReading reading = null;
            if(Id>=0)
                reading = collection.FindOneByIdAs<SensorReading>(Id);
            else
                reading = collection.FindAll().SetSortOrder(SortBy.Descending("date")).SetLimit(1).First<SensorReading>();
            return reading;
        }

        public HttpResponseMessage Post(SensorReading newReading)
        {
            Hub.Clients.All.addNewSensorReading(newReading);
            var response = Request.CreateResponse(HttpStatusCode.Created, newReading);


            var connectionString = "mongodb://localhost";
            var client = new MongoClient(connectionString);
            var server = client.GetServer();
            var db = server.GetDatabase("sensorData");
            var collection = db.GetCollection<SensorReading>("SensorReadings20");
            var _id = collection.Insert<SensorReading>(newReading);
            string link = Url.Link("Default", new { controller = "readings", id = _id });
            response.Headers.Location = new Uri(link);
            return response;
        }
    }
}
