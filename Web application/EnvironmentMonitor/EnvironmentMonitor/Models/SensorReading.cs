using MongoDB.Bson;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace WebApiApp.Models
{
    public class SensorReading
    {
        public double Reading { get; set; }
        public ObjectId  Id { get; set; }
        public int SensorId { get; set; }
        public string date { get; set; }
    }
}